#!/usr/bin/lua

local ev = require("ev")
local evmg = require("evmongoose")
local cjson = require("cjson")
local syslog = evmg.syslog

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
	local opt = syslog.LOG_ODELAY
	if log_to_stderr then
		opt = opt + syslog.LOG_PERROR 
	end
	syslog.openlog("xterminal device", opt, syslog.LOG_USER)
end

local function logger(level, ...)
	syslog.syslog(level, table.concat({...}, "\t"))
end

local function getopt(args, optstring, longopts)
	
	local program = args[0]
	local i = 1
	
	return function()
		local a = args[i]
		if not a then return nil end
		
		if a:sub(1, 2) == "--" then
			local name = a:sub(3)
			
			if not name or #name == 0 then
				i = i + 1
				return "?"
			end
			
			for _, v in ipairs(longopts) do
				if v[1] == name then
					local optarg = v[2] and args[i + 1] or nil
					
					if v[2] then
						if not optarg then
							print(program .. ":", "option requires an argument -- '" .. name .. "'")
							os.exit()
						end
						
						i = i + 1
					end
					
					i = i + 1
					return v[3], optarg, v[1]
				end
			end
			
			print(program .. ":", "invalid option -- '" .. name .. "'")
			
		elseif a:sub(1, 1) == "-" then
			local o = a:sub(2, 2)
			
			if not o or #o == 0 then
				i = i + 1
				return "?" 
			end
			
			if not optstring:match(o) then
				print(program .. ":", "invalid option -- '" .. o .. "'")
				os.exit()
			end
			
			local optarg
			if optstring:match(o .. ":") then
				if #a > 2 then
					optarg = a:sub(3)
				else
					optarg = args[i + 1]
					i = i + 1
				end
				
				if not optarg then
					print(program .. ":", "option requires an argument -- '" .. o .. "'")
					os.exit()
				end
			end
			
			i = i + 1
			return o, optarg
		else
			i = i + 1
			return "?"
		end
	end
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
		{"mqtt-port", true, 0},
		{"http-port", true, 0},
		{"document", true, 0},
		{"http-auth", true, 0},
		{"ssl-cert", true, 0},
		{"ssl-key", true, 0}
	}
	
	for o, optarg, lo in getopt(ARGV, "hsdc:", longopt) do
		if o == '?' or o == "h" then
			usage()
		end
		
		if o == "d" then
			log_to_stderr = true
		elseif o == "c" then
			conf = optarg
		elseif o == "s" then
			show = true
		elseif o == "0" then
			if lo == "mqtt-port" then
				mqtt_port = optarg
			elseif lo == "http-port" then
				http_port = optarg
			elseif lo == "document" then
				document_root = optarg
			elseif lo == "http-auth" then
				http_auth[#http_auth + 1] = optarg
			elseif lo == "ssl-cert" then
				ssl_cert = optarg
			elseif lo == "ssl-key" then
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

local upfile_name
local function upload_fname(fname)
	if fname:match("%s+") then return "" end
	upfile_name = fname
	return "/tmp/" .. fname
end

local mqtt_con
local mqtt_ping_timer

local function ev_handle(nc, event, msg)
	if event == evmg.MG_EV_CONNECT then
		mgr:set_protocol_mqtt(nc)
		mgr:send_mqtt_handshake_opt(nc, {clean_session = true})
		
	elseif event == evmg.MG_EV_MQTT_CONNACK then
		if msg.code ~= evmg.MG_EV_MQTT_CONNACK_ACCEPTED then
			logger(syslog.LOG_ERR, "Got mqtt connection error:", msg.code, msg.reason)
			return
		end
		
		mgr:mqtt_subscribe(nc, "xterminal/heartbeat/+");
		
		ev.Timer.new(function(loop, timer, revents)
			for k, v in pairs(device) do
				v.alive = v.alive - 1
				if v.alive == 0 then
					device[k] = nil
					logger(syslog.LOG_INFO, "timeout:", k)
				end
			end
		end, 1, 3):start(loop)
		
		mqtt_ping_timer = ev.Timer.new(function(loop, timer, revents) mgr:mqtt_ping(nc) end, 10, 30)
		mqtt_ping_timer:start(loop)
		
		logger(syslog.LOG_INFO, "connect mqtt on *:", mqtt_port, "ok")
		
	elseif event == evmg.MG_EV_MQTT_PUBLISH then
		local topic = msg.topic
		if topic:match("xterminal/heartbeat/%x+") then
			local mac = topic:match("xterminal/heartbeat/(%x+)")
			if not device[mac] then
				device[mac] = {mqtt_nc = nc}
				logger(syslog.LOG_INFO, "new dev:", mac)
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
			logger(syslog.LOG_INFO, "device close:", s.mac)
		elseif topic:match("xterminal/uploadfilefinish/%w+") then
			local sid = topic:match("xterminal/uploadfilefinish/(%w+)")
			if not session[sid] then return end
			local s = session[sid]
			local cmd = string.format("rm -f /tmp/%s %s/%s", upfile_name, document_root, upfile_name)
			os.execute(cmd)
			local rsp = {mt = "upfile"}
			mgr:send_websocket_frame(session[sid].websocket_nc, cjson.encode(rsp), evmg.WEBSOCKET_OP_TEXT)
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
				http_sessions[#http_sessions + 1] = {sid = sid, username = username, alive = 120}
				mgr:http_send_redirect(nc, 302, "/", string.format("Set-Cookie: mgs=%s;path=/", sid));
				
				logger(syslog.LOG_INFO, "login:", username, msg.remote_addr)
			else
				mgr:http_send_redirect(nc, 302, "/login.html")
			end
			return true
		end
		
		if upfile_name and uri:gsub("/", "") == upfile_name then return false end
		
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
			
			if #devs > 0 then
				mgr:print_http_chunk(nc, cjson.encode(devs))
			else
				mgr:print_http_chunk(nc, "[]")
			end
			mgr:print_http_chunk(nc, "")
			return true
		end
		
		local referer = msg.headers["Referer"]
		if referer and referer:match("/xterminal.html") or uri == "/xterminal.html" then
			return false
		end
		
		mgr:http_send_redirect(nc, 301, "/xterminal.html")
	
	elseif event == evmg.MG_EV_HTTP_MULTIPART_REQUEST_END and upfile_name then
		local cmd = string.format("rm -f %s/%s; ln -s /tmp/%s %s/%s", document_root, upfile_name, upfile_name, document_root, upfile_name)
		os.execute(cmd)
	elseif event == evmg.MG_EV_WEBSOCKET_HANDSHAKE_REQUEST then
		local mac = msg.query_string and msg.query_string:match("mac=([%x,:]+)") or ""
		mac = mac:gsub(":", ""):upper()
		session[generate_sid()] = {
			websocket_nc = nc,
			mac = mac
		}
		
		logger(syslog.LOG_INFO, "connect", mac, "by", msg.remote_addr)
	elseif event == evmg.MG_EV_WEBSOCKET_HANDSHAKE_DONE then
		local sid = find_sid_by_websocket(nc)
		local s = session[sid]
		local mac = s.mac
		local rsp = {mt = "connect", status = "ok"}
		
		if not validate_macaddr(mac) then
			session[sid] = nil
			rsp.status = "error"
			rsp.reason = "invalid macaddress"
		
		elseif not device[mac] then
			session[sid] = nil
			rsp.status = "error"
			rsp.reason = "device is offline"
		end
		
		mgr:send_websocket_frame(nc, cjson.encode(rsp), evmg.WEBSOCKET_OP_TEXT)
		if rsp.status == "error" then return end
		
		mgr:mqtt_subscribe(device[mac].mqtt_nc, "xterminal/touser/data/" .. sid);
		mgr:mqtt_subscribe(device[mac].mqtt_nc, "xterminal/touser/disconnect/" .. sid);
		mgr:mqtt_subscribe(device[mac].mqtt_nc, "xterminal/uploadfilefinish/" .. sid);
		
		mgr:mqtt_publish(device[mac].mqtt_nc, "xterminal/connect/" .. mac .. "/" .. sid, "");
			
	elseif event == evmg.MG_EV_WEBSOCKET_FRAME then
		local sid = find_sid_by_websocket(nc)
		local s = session[sid]
		if msg.op == evmg.WEBSOCKET_OP_BINARY then
			mgr:mqtt_publish(device[s.mac].mqtt_nc, "xterminal/todev/data/" .. sid, msg.data);
		elseif msg.op == evmg.WEBSOCKET_OP_TEXT then
			if msg.data and msg.data:match("upfile ") then
				local url = msg.data:match("upfile (http[s]?://[%w%.:]+)")
				if url then
					local data = string.format("%s %s", url, upfile_name)
					mgr:mqtt_publish(device[s.mac].mqtt_nc, "xterminal/uploadfile/" .. sid, data)
				else
					logger(syslog.LOG_ERR, "upfile invalid url:", msg.data)
				end
			end
		end
	elseif event == evmg.MG_EV_CLOSE then
		if nc == mqtt_con then
			if mqtt_ping_timer then
				mqtt_ping_timer:stop(loop)
				mqtt_ping_timer = nil
			end
			
			ev.Timer.new(function()
				logger(syslog.LOG_ERR, "Try MQTT Reconnect...")
				mqtt_con = mgr:connect(mqtt_port, ev_handle)
			end, 10):start(loop)
			return
		end
	
		local sid = find_sid_by_websocket(nc)
		local s = session[sid]
		
		if s then
			session[sid] = nil
			logger(syslog.LOG_INFO, "session close:", sid)
			
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

log_init()

mqtt_con = mgr:connect(mqtt_port, ev_handle)
logger(syslog.LOG_INFO, "Connect to mqtt broker", mqtt_port)

local nc = mgr:bind(http_port, ev_handle, {proto = "http", document_root = document_root, ssl_cert = ssl_cert, ssl_key = ssl_key})
mgr:set_fu_fname_fn(nc, upload_fname)
logger(syslog.LOG_INFO, "Listen on http", http_port)

ev.Signal.new(function(loop, sig, revents)
	loop:unloop()
end, ev.SIGINT):start(loop)

logger(syslog.LOG_INFO, "start...")

loop:loop()

mgr:destroy()

logger(syslog.LOG_INFO, "exit...")

syslog.closelog()