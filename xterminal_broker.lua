#!/usr/bin/lua

local ev = require("ev")
local evmg = require("evmongoose")
local cjson = require("cjson")
local syslog = require("syslog")

local loop = ev.Loop.default

local mgr = evmg.init()

local device = {}
local session = {}
local http_sessions = {}

local show = false
local log_to_stderr = false
local conf = "/etc/xterminal/xterminal.conf"
local mqtt_port = "1883"
local http_port = "8443"
local document_root = "www"
local ssl_cert = "server.pem"
local ssl_key = "server.key"
local http_auth = {}

local function logger(...)
	local opt = syslog.LOG_ODELAY
	if log_to_stderr then
		opt = opt + syslog.LOG_PERROR 
	end
	syslog.openlog("xterminal broker", opt, "LOG_USER")

	syslog.syslog(...)
	syslog.closelog()
end

local function usage()
	print(arg[0], "options")
	print("       -c 	             default /etc/xterminal/xterminal.conf")
	print("       -s                 Only show config")
	print("       -d                 Log to stderr")
	print("       --mqtt-port 	     default is 1883")
	print("       --http-port 	     default is 8000")
	print("       --document         default is ./www")
	print("       --http-auth        set http auth(username:password), default is xterminal:xterminal")
	print("       --ssl-cert 	     default is ./server.pem")
	print("       --ssl-key 	     default is ./server.key")
	
	os.exit()
end

local function parse_config()
	local f = io.open(conf, "r")
	if not f then return end
	f:close()
	
	local key, val
	for line in io.lines(conf) do
		if not line:match("^#") then
			key, val = line:match("([%w%-]+)=([%w%.:/_]+)")
			if key and val then
				if key == "mqtt-port" then
					mqtt_port = val
				elseif key == "http-port" then
					http_port = val
				elseif key == "document" then
					document_root = val
				elseif key == "http-auth" then
					http_auth[#http_auth + 1] = val
				elseif key == "ssl-cert" then	
					ssl_cert = val
				elseif key == "ssl-key" then	
					ssl_key = val
				end
			end
		end
	end
end

local function parse_commandline()
	local i = 1
	repeat
		if arg[i] == "-c" then
			if not arg[i + 1] then usage() end
			conf = arg[i + 1]
			i = i + 2
		elseif arg[i] == "--mqtt-port" then
			if not arg[i + 1] then usage() end
			mqtt_port = arg[i + 1]
			i = i + 2
		elseif arg[i] == "--http-port" then
			if not arg[i + 1] then usage() end
			http_port = arg[i + 1]
			i = i + 2
		elseif arg[i] == "--document" then
			if not arg[i + 1] then usage() end
			document_root = arg[i + 1]
			i = i + 2
		elseif arg[i] == "--http-auth" then
			if not arg[i + 1] then usage() end
			http_auth[#http_auth + 1] = arg[i + 1]
			i = i + 2
		elseif arg[i] == "--ssl-cert" then
			if not arg[i + 1] then usage() end
			ssl_cert = arg[i + 1]
			i = i + 2
		elseif arg[i] == "--ssl-key" then
			if not arg[i + 1] then usage() end
			ssl_key = arg[i + 1]
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
	print("mqtt-port:", mqtt_port)
	print("http-port:", http_port)
	print("document:", document_root)
	print("ssl-cert:", ssl_cert)
	print("ssl-key:", ssl_key)
	for _, v in ipairs(http_auth) do
		print("http-auth:", v)
	end
	os.exit()
end

local function verify_http_auth(username, password)
	local auth = username .. ":" .. password 
	for _, v in ipairs(http_auth) do
		if v == auth then return true end
	end
	return false
end

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

local function find_http_session(sid)
	for _, s in ipairs(http_sessions) do
		if s.sid == sid then
			s.alive = 6
			return s
		end
	end

	return nil
end

local function del_http_session(sid)
	for _, s in ipairs(http_sessions) do
		if s.sid == sid then
			table.remove(http_sessions, i)
		end
	end

	return nil
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
		mgr:send_mqtt_handshake_opt(nc, {clean_session = true})
		
	elseif event == evmg.MG_EV_MQTT_CONNACK then
		if msg.connack_ret_code ~= evmg.MG_EV_MQTT_CONNACK_ACCEPTED then
			logger("LOG_ERR", "Got mqtt connection error: " .. msg.connack_ret_code)
			return
		end
		
		mgr:mqtt_subscribe(nc, "xterminal/heartbeat/+");
		
		ev.Timer.new(function(loop, timer, revents)
			for k, v in pairs(device) do
				v.alive = v.alive - 1
				if v.alive == 0 then
					device[k] = nil
					logger("LOG_INFO", "timeout: " .. k)
				end
			end
		end, 1, 3):start(loop)
		
		ev.Timer.new(function(loop, timer, revents)
			mgr:mqtt_ping(nc)
		end, 10, 10):start(loop)
		
	elseif event == evmg.MG_EV_MQTT_PUBLISH then
		local topic = msg.topic
		if topic:match("xterminal/heartbeat/%x+") then
			local mac = topic:match("xterminal/heartbeat/(%x+)")
			if not device[mac] then
				device[mac] = {mqtt_nc = nc}
				logger("LOG_INFO", "new dev:" .. mac)
			end
			device[mac].alive = 5
		elseif topic:match("xterminal/touser/data/%w+") then
			local sid = topic:match("xterminal/touser/data/(%w+)")
			if not session[sid] then return end
			mgr:send_websocket_frame(session[sid].websocket_nc, msg.payload, evmg.WEBSOCKET_OP_BINARY)
		elseif topic:match("xterminal/touser/disconnect/%w+") then
			local sid = topic:match("xterminal/touser/disconnect/(%w+)")
			if not session[sid] then return end
			local s = session[sid]
			
			mgr:send_websocket_frame(session[sid].websocket_nc, "", evmg.WEBSOCKET_OP_CLOSE)
			logger("LOG_INFO", "device close:" .. s.mac)
		end
		
	elseif event == evmg.MG_EV_HTTP_REQUEST then
		local uri = msg.uri
		
		if uri:match("/%w+%.js") or uri:match("/%w+%.css") then return false end
		
		if uri == "/login.html" then
			if msg.method ~= "POST" then return end
			
			local username = mgr:get_http_var(msg.hm, "username") or ""
			local password = mgr:get_http_var(msg.hm, "password") or ""
			
			if verify_http_auth(username, password) then
				local sid = generate_sid()
				http_sessions[#http_sessions + 1] = {sid = sid, alive = 120}
				mgr:http_send_redirect(nc, 302, "/", "Set-Cookie: mgs=" .. sid .. "; path=/");
				
				logger("LOG_INFO", "login:" .. username .. ":" .. msg.headers["Host"])
			else
				mgr:http_send_redirect(nc, 302, "/login.html")
			end
			return true
		end
		
		local cookie = msg.headers["Cookie"] or ""
		local sid = cookie:match("mgs=(%w+)")

		if not find_http_session(sid) then
			mgr:http_send_redirect(nc, 302, "/login.html")
			return true
		end
		
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
		local mac = msg.query_string and msg.query_string:match("mac=([%x,:]+)") or ""
		mac = mac:gsub(":", ""):upper()
		session[generate_sid()] = {
			websocket_nc = nc,
			mac = mac
		}
		
		logger("LOG_INFO", "connect '" .. mac .. "' by " .. msg.headers["Host"])
	elseif event == evmg.MG_EV_WEBSOCKET_HANDSHAKE_DONE then
		local sid = find_sid_by_websocket(nc)
		local s = session[sid]
		local mac = s.mac
		if not validate_macaddr(mac) then
			session[sid] = nil
			mgr:send_websocket_frame(nc, cjson.encode({status = "error", reason = "invalid macaddress"}), evmg.WEBSOCKET_OP_TEXT)
			return
		end
		
		if not device[mac] then
			session[sid] = nil
			mgr:send_websocket_frame(nc, cjson.encode({status = "error", reason = "device not online"}), evmg.WEBSOCKET_OP_TEXT)
			return
		end
		
		mgr:send_websocket_frame(nc, cjson.encode({status = "ok"}), evmg.WEBSOCKET_OP_TEXT)
			
		mgr:mqtt_subscribe(device[mac].mqtt_nc, "xterminal/touser/data/" .. sid);
		mgr:mqtt_subscribe(device[mac].mqtt_nc, "xterminal/touser/disconnect/" .. sid);
		mgr:mqtt_publish(device[mac].mqtt_nc, "xterminal/connect/" .. mac .. "/" .. sid, "");
			
	elseif event == evmg.MG_EV_WEBSOCKET_FRAME then
		local sid = find_sid_by_websocket(nc)
		local s = session[sid]
		mgr:mqtt_publish(device[s.mac].mqtt_nc, "xterminal/todev/data/" .. sid, msg.data);
	elseif event == evmg.MG_EV_CLOSE then
		local sid = find_sid_by_websocket(nc)
		local s = session[sid]
		
		if s then
			session[sid] = nil
			logger("LOG_INFO", "session close: " .. sid)
			
			if device[s.mac] then
				mgr:mqtt_publish(device[s.mac].mqtt_nc, "xterminal/todev/disconnect/" .. sid, "");
			end
		end
	end
end

ev.Timer.new(function(loop, timer, revents)
	for i, s in ipairs(http_sessions) do
		s.alive = s.alive - 1
		if s.alive == 0 then
			table.remove(http_sessions, i)
		end
	end
end, 5, 5):start(loop)

math.randomseed(tostring(os.time()):reverse():sub(1, 6))

parse_commandline()
parse_config()
if #http_auth == 0 then http_auth[1] = "xterminal:xterminal" end
if show then show_conf() end

mgr:connect(mqtt_port, ev_handle)
logger("LOG_INFO", "Connect to mqtt broker " .. mqtt_port .. "....")

mgr:bind(http_port, ev_handle, {proto = "http", document_root = document_root, ssl_cert = ssl_cert, ssl_key = ssl_key})
logger("LOG_INFO", "Listen on http " .. http_port .. "....")

ev.Signal.new(function(loop, sig, revents)
	loop:unloop()
end, ev.SIGINT):start(loop)

logger("LOG_INFO", "start...")

loop:loop()

mgr:destroy()

logger("LOG_INFO", "exit...")