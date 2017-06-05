#!/usr/bin/lua

local ev = require("ev")
local evmg = require("evmongoose")
local posix = require 'posix'
local cjson = require("cjson")

local loop = ev.Loop.default

local mgr = evmg.init()

local device = {}
local session = {}

local function find_sid_by_websocket(nc)
	for k, v in pairs(session) do
		if v.websocket_nc == nc then
			return k
		end
	end
	return nil
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
		
		mgr:mqtt_subscribe(nc, "xterminal/heartbeat");
		
		ev.Timer.new(function(loop, timer, revents)
			for k, v in pairs(device) do
				v.alive = v.alive - 1
				if v.alive == 0 then
					device[k] = nil
					print("timeout:", k)
				end
			end
		end, 1, 3):start(loop)
		
	elseif event == evmg.MG_EV_MQTT_PUBLISH then
		local topic = msg.topic
		print("mqtt recv:", topic)
		if topic == "xterminal/heartbeat" then
			local mac = msg.payload
			if not device[mac] then
				device[mac] = {}
				print("new dev:", mac)
			end
			device[mac] = {alive = 5, mqtt_nc = nc}
		elseif topic:match("devdata") then
			local mac, sid = topic:match("xterminal/(%w+)/(%w+)/devdata")
			mgr:send_websocket_frame(session[sid].websocket_nc, msg.payload)
		end
		
	elseif event == evmg.MG_EV_HTTP_REQUEST then
		local uri = msg.uri
		if uri == "/list" then
			mgr:send_head(nc, 200, -1)
			
			local devs = {}
			
			for k, v in pairs(device) do
				devs[#devs + 1] = k
			end
			
			mgr:print_http_chunk(nc, cjson.encode(devs))
			mgr:print_http_chunk(nc, "")
			return true
		end
		
	elseif event == evmg.MG_EV_WEBSOCKET_FRAME then
		local data = msg.data
		if data:match("connect ") then
			local mac = data:match("connect (%w+)")
			local sid = "fsdg23423"
			
			session[sid] = {
				websocket_nc = nc,
				mac = mac
			}
			
			mgr:mqtt_subscribe(device[mac].mqtt_nc, "xterminal/" .. mac ..  "/" .. sid .. "/devdata");
			mgr:mqtt_publish(device[mac].mqtt_nc, "xterminal/" .. mac .. "/connect/" .. sid, "");
		else
			local sid = find_sid_by_websocket(nc)
			local s = session[sid]
			mgr:mqtt_publish(device[s.mac].mqtt_nc, "xterminal/" .. s.mac ..  "/" .. sid .. "/srvdata", msg.data);
		end
	end
end

mgr:connect("localhost:1883", ev_handle)

mgr:bind("8000", ev_handle, {proto = "http"})
print("Listen on http 8000...")

ev.Signal.new(function(loop, sig, revents)
	loop:unloop()
end, ev.SIGINT):start(loop)

print("start...")

loop:loop()

mgr:destroy()

print("exit...")