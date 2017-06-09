#!/usr/bin/lua

local ev = require("ev")
local evmg = require("evmongoose")
local posix = require 'posix'
local cjson = require("cjson")

local loop = ev.Loop.default

local mgr = evmg.init()

local device = {}
local session = {}
local document_root = arg[1] or "www"

local function generate_sid()
	local t = {}
	for i = 1, 5 do
		t[#t + 1] = string.char(math.random(65, 90))
	end
	
	for i = 1, 5 do
		t[#t + 1] = string.char(math.random(48, 57))
	end
	
	return table.concat(t)
end

function validate_macaddr(val)
	if val then
		if #val == 17 and (val:match("^%x+:%x+:%x+:%x+:%x+:%x+$") or val:match("^%x+%-%x+%-%x+%-%x+%-%x+%-%x+$")) then
			return true
		end
		
		if #val == 12 and val:match("^%x+%x+%x+%x+%x+%x$") then
			return true
		end
	end

	return false
end

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
		if topic == "xterminal/heartbeat" then
			local mac = msg.payload
			if not device[mac] then
				device[mac] = {mqtt_nc = nc}
				print("new dev:", mac)
			end
			device[mac].alive = 5
		elseif topic:match("devdata") then
			local mac, sid = topic:match("xterminal/(%w+)/(%w+)/devdata")
			if not session[sid] then return end
			mgr:send_websocket_frame(session[sid].websocket_nc, msg.payload, evmg.WEBSOCKET_OP_BINARY)
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
		
		local referer = msg.headers["Referer"]
		if referer and referer:match("/xterminal.html") or uri == "/xterminal.html" then
			return false
		end
		
		mgr:http_send_redirect(nc, 301, "/xterminal.html")
	
	elseif event == evmg.MG_EV_WEBSOCKET_HANDSHAKE_REQUEST then
		session[generate_sid()] = {
			websocket_nc = nc,
			mac = msg.query_string:match("mac=([%x,:]+)"):gsub(":", ""):upper()
		}
	elseif event == evmg.MG_EV_WEBSOCKET_HANDSHAKE_DONE then
		local sid = find_sid_by_websocket(nc)
		local s = session[sid]
		local mac = s.mac
		if not validate_macaddr(mac) then
			mgr:send_websocket_frame(nc, cjson.encode({status = "error", reason = "invalid macaddress"}))
			return
		end
		
		if not device[mac] then
			mgr:send_websocket_frame(nc, cjson.encode({status = "error", reason = "device not online"}))
			return
		end
		
		mgr:send_websocket_frame(nc, cjson.encode({status = "ok"}))
			
		mgr:mqtt_subscribe(device[mac].mqtt_nc, "xterminal/" .. mac ..  "/" .. sid .. "/devdata");
		mgr:mqtt_publish(device[mac].mqtt_nc, "xterminal/" .. mac .. "/connect/" .. sid, "");
			
	elseif event == evmg.MG_EV_WEBSOCKET_FRAME then
		local sid = find_sid_by_websocket(nc)
		local s = session[sid]
		mgr:mqtt_publish(device[s.mac].mqtt_nc, "xterminal/" .. s.mac ..  "/" .. sid .. "/srvdata", msg.data);
	elseif event == evmg.MG_EV_CLOSE then
		local sid = find_sid_by_websocket(nc)
		local s = session[sid]
		
		if s then
			session[sid] = nil
			print("session close:", nc, sid)
			if device[s.mac] then
				mgr:mqtt_publish(device[s.mac].mqtt_nc, "xterminal/" .. s.mac ..  "/" .. sid .. "/exit", "");
			end
		end
	end
end

math.randomseed(tostring(os.time()):reverse():sub(1, 6))

mgr:connect("1883", ev_handle)

mgr:bind("8000", ev_handle, {proto = "http", document_root = document_root})
print("Listen on http 8000...")

ev.Signal.new(function(loop, sig, revents)
	loop:unloop()
end, ev.SIGINT):start(loop)

print("start...")

loop:loop()

mgr:destroy()

print("exit...")
