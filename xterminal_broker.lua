#!/usr/bin/lua

local ev = require("ev")
local evmg = require("evmongoose")
local cjson = require("cjson")
local posix = evmg.posix
local loop = ev.Loop.default

local mgr = evmg.init()

local device = {}
local session = {}
local http_sessions = {}

local ARGV = arg
local show = false
local log_to_stderr = false
local conf = "/etc/xterminal/xterminal.conf"
local mqtt_port = "1883"
local http_port = "8443"
local document_root = "www"
local ssl_cert = "server.pem"
local ssl_key = "server.key"
local http_auth = {}

local function log_init()
	local opt = posix.LOG_ODELAY
	if log_to_stderr then
		opt = opt + posix.LOG_PERROR 
	end
	posix.openlog("xterminal broker", opt, posix.LOG_USER)
end

local function logger(level, ...)
	posix.syslog(level, table.concat({...}, "  "))
end

local function usage()
	print("Usage:", ARGV[0], "options")
	print([[
        -c 	             default /etc/xterminal/xterminal.conf
        -s               Only show config
        -d               Log to stderr
        --mqtt-port 	 default is 1883
        --http-port 	 default is 8000
        --document       default is ./www
        --http-auth      set http auth(username:password), default is xterminal:xterminal
        --ssl-cert       default is ./server.pem
        --ssl-key        default is ./server.key
	]])
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
	local longopt = {
		{"help",  nil, 'h'},
		{"mqtt-port", true, "0"},
		{"http-port", true, "0"},
		{"document", true, "0"},
		{"http-auth", true, "0"},
		{"ssl-cert", true, "0"},
		{"ssl-key", true, "0"}
	}
	
	for o, optarg, longindex in posix.getopt(ARGV, "hsdc:", longopt) do
		if o == "d" then
			log_to_stderr = true
		elseif o == "c" then
			conf = optarg
		elseif o == "s" then
			show = true
		elseif o == "0" then
			local name = longopt[longindex][1]
			if name == "mqtt-port" then
				mqtt_port = optarg
			elseif name == "http-port" then
				http_port = optarg
			elseif name == "document" then
				document_root = optarg
			elseif name == "http-auth" then
				http_auth[#http_auth + 1] = optarg
			elseif name == "ssl-cert" then
				ssl_cert = optarg
			elseif name == "ssl-key" then
				ssl_key = optarg
			end
		else
			usage()
		end
	end
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

local function generate_sid(con)
	return evmg.cs_md5(con:tostring(), string.format("%f", evmg.mg_time()))
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

local function del_session(s)
	for k, v in ipairs(session) do
		if v.sid == s.sid then
			table.remove(session, k)
			break
		end
	end
end

local function find_session_by_websocket(con)
	for _, v in ipairs(session) do
		if v.con == con then
			return v
		end
	end
	return nil
end

local function find_session_by_sid(sid)
	for _, v in ipairs(session) do
		if v.sid == sid then
			return v
		end
	end
	return nil
end

local uploadfile_name

local function http_ev_handle(con, event)
	if event == evmg.MG_EV_HTTP_REQUEST then
		local hm = con:get_evdata()
		local uri = hm.uri
		
		if uri:match("/%w+%.js") or uri:match("/%w+%.css") then return false end
		
		if uri == "/login.html" then
			if hm.method ~= "POST" then return end
			
			local username = con:get_http_var("username") or ""
			local password = con:get_http_var("password") or ""
			
			if verify_http_auth(username, password) then
				local sid = generate_sid(con)
				http_sessions[#http_sessions + 1] = {sid = sid, username = username, alive = 120}
				con:send_http_redirect(302, "/", string.format("Set-Cookie: mgs=%s;path=/", sid));
				
				logger(posix.LOG_INFO, "login:", username, hm.remote_addr)
			else
				con:send_http_redirect(302, "/login.html")
			end
			return true
		end
		
		if uploadfile_name and uri:gsub("/", "") == uploadfile_name then return false end
		
		local headers = con:get_http_headers()
		local cookie = headers["Cookie"] or ""
		local sid = cookie:match("mgs=(%w+)")
		local s = find_http_session(sid)
		if not s then
			con:send_http_redirect(302, "/login.html")
			return true
		end
		
		if uri == "/list" then
			con:send_http_head(200, -1)
			
			local devs = {}
			for k, v in pairs(device) do
				devs[#devs + 1] = k
			end
			
			if #devs > 0 then
				con:send_http_chunk(cjson.encode(devs))
			else
				con:send_http_chunk("[]")
			end
			con:send_http_chunk("")
			return true
		end
		
		local referer = headers["Referer"]
		if referer and referer:match("/xterminal.html") or uri == "/xterminal.html" then
			return false
		end
		
		con:send_http_redirect(301, "/xterminal.html")
	
	elseif event == evmg.MG_EV_HTTP_PART_BEGIN then
		local part = con:get_evdata()
		local file_name = part.file_name
		if not file_name or #file_name == 0 or file_name:match("[^%w%-%._]+") then
			con:send("HTTP/1.1 500 Internal Server Error\r\nContent-Type: text/plain\r\nConnection: close\r\n\r\nInvalid filename");
			-- If the false is returned, it continues to be processed by the underlying layer, otherwise finish end processing
			return true
		end
		
	elseif event == evmg.MG_EV_HTTP_PART_END then
		local part = con:get_evdata()
		if part.lfn then
			os.rename(part.lfn, document_root .. "/" .. part.file_name)
			uploadfile_name = part.file_name
		end
		
	elseif event == evmg.MG_EV_WEBSOCKET_HANDSHAKE_REQUEST then
		local hm = con:get_evdata()
		local query_string = hm.query_string
		local mac = query_string and query_string:match("mac=([%x,:]+)") or ""
		mac = mac:gsub(":", ""):upper()
		local sid = generate_sid(con)
		session[#session + 1] = {
			con = con,
			mac = mac,
			sid = sid
		}
		
		logger(posix.LOG_INFO, "connect", mac, "by", hm.remote_addr, "new session:", sid)
	elseif event == evmg.MG_EV_WEBSOCKET_HANDSHAKE_DONE then
		local s = find_session_by_websocket(con)
		local mac = s.mac
		local rsp = {mt = "connect", status = "ok"}
		
		if not validate_macaddr(mac) then
			del_session(s)
			rsp.status = "error"
			rsp.reason = "invalid macaddress"
		
		elseif not device[mac] then
			del_session(s)
			rsp.status = "error"
			rsp.reason = "device is offline"
		end
		
		con:send_websocket_frame(cjson.encode(rsp), evmg.WEBSOCKET_OP_TEXT)
		if rsp.status == "error" then return end
		
		local dev_con = device[mac].con
		local topic = {
			{name = "xterminal/touser/data/" .. s.sid},
			{name = "xterminal/touser/disconnect/" .. s.sid},
			{name = "xterminal/uploadfilefinish/" .. s.sid}
		}
		
		dev_con:mqtt_subscribe(topic);
		dev_con:mqtt_publish("xterminal/connect/" .. mac .. "/" .. s.sid, "");
			
	elseif event == evmg.MG_EV_WEBSOCKET_FRAME then
		local msg = con:get_evdata()
		local s = find_session_by_websocket(con)
		if not s then
			con:send_websocket_frame("", evmg.WEBSOCKET_OP_CLOSE)
			return
		end
		
		local op = msg.op
		local data = msg.frame
		local dev_con = device[s.mac].con
		
		if op == evmg.WEBSOCKET_OP_BINARY then
			dev_con:mqtt_publish("xterminal/todev/data/" .. s.sid, data);
		elseif op == evmg.WEBSOCKET_OP_TEXT then
			if data and data:match("upfile ") then
				local url = data:match("upfile (http[s]?://[%w%.:]+)")
				if url and uploadfile_name then
					local data = string.format("%s %s", url, uploadfile_name)
					dev_con:mqtt_publish("xterminal/uploadfile/" .. s.sid, data)
				else
					logger(posix.LOG_ERR, "upfile invalid url:", data)
				end
			end
		end
	elseif event == evmg.MG_EV_CLOSE then
		if con:is_websocket() then
			local s = find_session_by_websocket(con)
			if s then
				del_session(s)
				logger(posix.LOG_INFO, "session close:", s.sid)
				
				if device[s.mac] then
					device[s.mac].con:mqtt_publish("xterminal/todev/disconnect/" .. s.sid, "");
				end
			end
		end
	end
end


local mqtt_keep_alive = 3
local mqtt_alive_timer = ev.Timer.new(function(loop, w, event)
	mqtt_keep_alive = mqtt_keep_alive - 1
	if mqtt_keep_alive == 0 then w:stop(loop) end
end, 5, 5)

local function mqtt_ev_handle(con, event)
	if event == evmg.MG_EV_CONNECT then
		local result = con:get_evdata()
		if not result.connected then
			logger(posix.LOG_ERR, "connect failed:", result.err)
		else
			con:mqtt_handshake({clean_session = true})
		end
		
	elseif event == evmg.MG_EV_MQTT_CONNACK then
		local msg = con:get_evdata()
		if msg.code ~= evmg.MG_EV_MQTT_CONNACK_ACCEPTED then
			logger(posix.LOG_ERR, "Got mqtt connection error:", msg.code, msg.err)
			return
		end
		
		local topic = {
			{name = "xterminal/heartbeat/+"}
		}
		con:mqtt_subscribe(topic);
		mqtt_alive_timer:start(loop)
		logger(posix.LOG_INFO, "connect mqtt on *:", mqtt_port, "ok")
		
	elseif event == evmg.MG_EV_MQTT_PUBLISH then
		local msg = con:get_evdata()
		local topic, payload = msg.topic, msg.payload
		if topic:match("xterminal/heartbeat/%x+") then
			local mac = topic:match("xterminal/heartbeat/(%x+)")
			if not device[mac] then
				device[mac] = {con = con}
				logger(posix.LOG_INFO, "new dev:", mac)
			end
			device[mac].alive = 5
		elseif topic:match("xterminal/touser/data/%w+") then
			local sid = topic:match("xterminal/touser/data/(%w+)")
			local s = find_session_by_sid(sid)
			if not s then return end
			s.con:send_websocket_frame(payload, evmg.WEBSOCKET_OP_BINARY)
		elseif topic:match("xterminal/touser/disconnect/%w+") then
			local sid = topic:match("xterminal/touser/disconnect/(%w+)")
			local s = find_session_by_sid(sid)
			if not s then return end
			
			s.con:send_websocket_frame("", evmg.WEBSOCKET_OP_CLOSE)
			logger(posix.LOG_INFO, "device close:", s.mac)
		elseif topic:match("xterminal/uploadfilefinish/%w+") then
			if uploadfile_name then
				os.execute("rm -f " .. document_root .. "/" .. uploadfile_name)
			end
		end
		
	elseif event == evmg.MG_EV_MQTT_PINGRESP then
		mqtt_keep_alive = 3
		
	elseif event == evmg.MG_EV_POLL then
		if mqtt_keep_alive == 0 then
			-- Disconnect and reconnection
			logger(posix.LOG_ERR, "mqtt_keep_alive timeout")
			con:set_flags(evmg.MG_F_CLOSE_IMMEDIATELY)
			mqtt_keep_alive = 3
		end
	elseif event == evmg.MG_EV_CLOSE then
		ev.Timer.new(function()
			logger(posix.LOG_ERR, "Try MQTT Reconnect...")
			mgr:connect(mqtt_ev_handle, mqtt_port)
		end, 10):start(loop)
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

ev.Timer.new(function(loop, timer, revents)
	for k, v in pairs(device) do
		v.alive = v.alive - 1
		if v.alive == 0 then
			device[k] = nil
			logger(posix.LOG_INFO, "timeout:", k)
		end
	end
end, 1, 3):start(loop)
		
math.randomseed(tostring(os.time()):reverse():sub(1, 6))

parse_commandline()
parse_config()
if #http_auth == 0 then http_auth[1] = "xterminal:xterminal" end
if show then show_conf() end

log_init()

mgr:connect(mqtt_ev_handle, mqtt_port)
logger(posix.LOG_INFO, "Connect to mqtt broker", mqtt_port)

mgr:listen(http_ev_handle, http_port, {proto = "http", document_root = document_root, ssl_cert = ssl_cert, ssl_key = ssl_key})
logger(posix.LOG_INFO, "Listen on http", http_port)

ev.Signal.new(function(loop, sig, revents)
	loop:unloop()
end, ev.SIGINT):start(loop)

logger(posix.LOG_INFO, "start...")

loop:loop()

logger(posix.LOG_INFO, "exit...")

posix.closelog()