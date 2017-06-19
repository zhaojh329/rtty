#!/usr/bin/lua

local ev = require("ev")
local evmg = require("evmongoose")
local posix = require 'posix'
local cjson = require("cjson")
local syslog = require("syslog")

local loop = ev.Loop.default
local mgr = evmg.init()

local show = false
local log_to_stderr = false
local ifname = "eth0"
local mqtt_host = "localhost"
local mqtt_port = "1883"
local keepalive = 0
local devid = nil
local session = {}

local function logger(...)
	local opt = syslog.LOG_ODELAY
	if log_to_stderr then
		opt = opt + syslog.LOG_PERROR 
	end
	syslog.openlog("xterminal device", opt, "LOG_USER")
	syslog.syslog(...)
	syslog.closelog()
end

local function usage()
	print(arg[0], "options")
	print("       -s              Only show config")
	print("       -d              Log to stderr")
	print("       -i 	          default is eth0")
	print("       --mqtt-port 	  default is 1883")
	print("       --mqtt-host 	  default is localhost")
	os.exit()
end

local function parse_commandline()
	local i = 1
	repeat
		if arg[i] == "--mqtt-port" then
			if not arg[i + 1] then usage() end
			mqtt_port = arg[i + 1]
			i = i + 2
		elseif arg[i] == "--mqtt-host" then
			if not arg[i + 1] then usage() end
			mqtt_host = arg[i + 1]
			i = i + 2
		elseif arg[i] == "-i" then
			if not arg[i + 1] then usage() end
			ifname = arg[i + 1]
			i = i + 2
		elseif arg[i] == "-s" then
			show = true
			i = i + 1
		elseif arg[i] == "-d" then
			log_to_stderr = true
			i = i + 1
		else
			i = i + 1
		end
	until(not arg[i])
end

local function show_conf()
	print("mqtt-port", mqtt_port)
	print("mqtt-host", mqtt_host)
	print("ifname", ifname)
	os.exit()
end

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
	
	mgr:mqtt_subscribe(nc, "xterminal/todev/data/" .. id);
	mgr:mqtt_subscribe(nc, "xterminal/todev/disconnect/" .. id);
	
	session[id].rio = ev.IO.new(function(loop, w, revents)
		local d, err = posix.read(w:getfd(), 1024)
		if not d then
			w:stop(loop)
			posix.wait(pid)
			session[id] = nil
			mgr:mqtt_publish(nc, "xterminal/touser/disconnect/" .. id, "");
			return
		end
		mgr:mqtt_publish(nc, "xterminal/touser/data/" .. id, d);
	end, pty, ev.READ)
	
	session[id].rio:start(loop)
end

local function ev_handle(nc, event, msg)
	if event == evmg.MG_EV_CONNECT then
		mgr:set_protocol_mqtt(nc)
		mgr:send_mqtt_handshake_opt(nc, {clean_session = true})
		
	elseif event == evmg.MG_EV_MQTT_CONNACK then
		if msg.connack_ret_code ~= evmg.MG_EV_MQTT_CONNACK_ACCEPTED then
			logger("LOG_ERR", "Got mqtt connection error: " .. msg.connack_ret_code)
			return
		end
		
		mgr:mqtt_subscribe(nc, "xterminal/connect/" .. devid ..  "/+");
		
		ev.Timer.new(function(loop, timer, revents)
			mgr:mqtt_publish(nc, "xterminal/heartbeat/" .. devid, "")
		end, 0.1, 3):start(loop)
		
		ev.Timer.new(function(loop, timer, revents)
			mgr:mqtt_ping(nc)
		end, 1, 10):start(loop)
		
		logger("LOG_INFO", "connect " .. mqtt_host .. ":" .. mqtt_port .. " ok")
		
	elseif event == evmg.MG_EV_MQTT_PUBLISH then
		local topic = msg.topic
		if topic:match("xterminal/connect/%w+") then
			local id = topic:match("xterminal/connect/" .. devid .. "/(%w+)")
			logger("LOG_INFO", "New connection: " .. id)
			new_connect(nc, id)
		elseif topic:match("xterminal/todev/data/%w+") then
			local id = topic:match("xterminal/todev/data/(%w+)")
			posix.write(session[id].pty, msg.payload)
		elseif topic:match("xterminal/todev/disconnect/%w+") then
			local id = topic:match("xterminal/todev/disconnect/(%w+)")
			if session[id] then
				logger("LOG_INFO", "connection close: " .. id)
				posix.kill(session[id].pid)
				session[id].rio:stop(loop)
			end
		end
	elseif event == evmg.MG_EV_MQTT_PINGRESP then
		keepalive = 3
	end
end

parse_commandline()
if show then show_conf() end

devid = get_dev_id(ifname)
if not devid then
	print("get dev id failed for", ifname)
	logger("LOG_ERR", "get dev id failed for " .. ifname)
	os.exit()
end

logger("LOG_INFO", "devid: " .. devid)

mgr:connect(mqtt_host .. ":" .. mqtt_port, ev_handle)

ev.Signal.new(function(loop, sig, revents)
	loop:unloop()
end, ev.SIGINT):start(loop)

logger("LOG_INFO", "start...")

ev.Timer.new(function(loop, timer, revents)
	if keepalive == 0 then
		logger("LOG_INFO", "re connect......")
		mgr:connect(mqtt_host .. ":" .. mqtt_port, ev_handle)
	else
		keepalive = keepalive - 1
	end
end, 10, 10):start(loop)
		
loop:loop()

mgr:destroy()

logger("LOG_INFO", "exit...")