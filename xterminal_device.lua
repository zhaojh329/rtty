#!/usr/bin/lua

local ev = require("ev")
local evmg = require("evmongoose")
local posix = require 'posix'
local cjson = require("cjson")

local loop = ev.Loop.default

local mgr = evmg.init()
local ifname = "ens38"
local devid = nil
local session = {}

local function get_dev_id(ifname)
	local file = io.open("/sys/class/net/" .. ifname .. "/address", "r")
	if not file then return nil end
	local d = file:read("*l")
	file:close()
	
	if not d then return nil end
	
	return d:gsub(":", ""):upper()
end

local function new_connect(nc, id)
	local pid, pty = evmg.forkpty()
	if pid == 0 then posix.exec ("/bin/login", {}) end
	
	session[id] = {pid = pid, pty = pty}
	
	mgr:mqtt_subscribe(nc, "xterminal/" .. devid .. "/" .. id .. "/srvdata");
	mgr:mqtt_subscribe(nc, "xterminal/" .. devid .. "/" .. id .. "/exit");
	
	local topic_dev = "xterminal/" .. devid .. "/" .. id ..  "/devdata"
	
	session[id].rio = ev.IO.new(function(loop, w, revents)
		local d, err = posix.read(w:getfd(), 1024)
		if not d then
			w:stop(loop)
			posix.wait(pid)
			session[id] = nil
			return
		end
		mgr:mqtt_publish(nc, topic_dev, d);
	end, pty, ev.READ)
	
	session[id].rio:start(loop)
end

local function ev_handle(nc, event, msg)
	if event == evmg.MG_EV_CONNECT then
		mgr:set_protocol_mqtt(nc)
		mgr:send_mqtt_handshake_opt(nc)
		
	elseif event == evmg.MG_EV_MQTT_CONNACK then
		if msg.connack_ret_code ~= evmg.MG_EV_MQTT_CONNACK_ACCEPTED then
			print("Got mqtt connection error:", msg.connack_ret_code)
			return
		end
		
		mgr:mqtt_subscribe(nc, "xterminal/" .. devid ..  "/connect/+");
		
		ev.Timer.new(function(loop, timer, revents)
			mgr:mqtt_publish(nc, "xterminal/heartbeat", devid)
		end, 0.1, 3):start(loop)
		
	elseif event == evmg.MG_EV_MQTT_PUBLISH then
		local topic = msg.topic
		if topic:match("connect") then
			local id = topic:match("xterminal/" .. devid .. "/connect/(%w+)")
			new_connect(nc, id)
		elseif topic:match("srvdata") then
			local id = topic:match("xterminal/" .. devid .. "/(%w+)/srvdata")
			posix.write(session[id].pty, msg.payload)
		elseif topic:match("exit") then
			local id = topic:match("xterminal/" .. devid .. "/(%w+)/exit")
			if session[id] then
				posix.kill(session[id].pid)
				session[id].rio:stop(loop)
			end
		end
	end
end

devid = get_dev_id(ifname)
if not devid then
	print("get dev id failed for", ifname)
	os.exit()
end

print("devid:", devid)

mgr:connect("localhost:1883", ev_handle)

ev.Signal.new(function(loop, sig, revents)
	loop:unloop()
end, ev.SIGINT):start(loop)

print("start...")

loop:loop()

mgr:destroy()

print("exit...")