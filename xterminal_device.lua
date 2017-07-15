#!/usr/bin/lua

local ev = require("ev")
local evmg = require("evmongoose")
local cjson = require("cjson")
local posix = evmg.posix
local loop = ev.Loop.default

local ARGV = arg
local show = false
local log_to_stderr = false
local ifname = "eth0"
local mqtt_host = "localhost"
local mqtt_port = "1883"
local devid = nil
local session = {}

local function log_init()
	local opt = posix.LOG_ODELAY
	if log_to_stderr then
		opt = opt + posix.LOG_PERROR 
	end
	posix.openlog("xterminal device", opt, posix.LOG_USER)
end

local function logger(level, ...)
	posix.syslog(level, table.concat({...}, "  "))
end

local function usage()
	print("Usage:", ARGV[0], "options")
	print([[
        -s              Only show config
        -d              Log to stderr
        -i              default is eth0
        --mqtt-port     default is 1883
        --mqtt-host 	default is localhost
	]])
	os.exit()
end

local function parse_commandline()
	local longopt = {
		{"help",  false, 'h'},
		{"mqtt-port", true, "0"},
		{"mqtt-host", true, "0"}
	}
	
	for r, optarg, longindex in posix.getopt(ARGV, "hsdi:", longopt) do
		if r == "d" then
			log_to_stderr = true
		elseif r == "i" then
			ifname = optarg
		elseif r == "s" then
			show = true
		elseif r == "0" then
			local name = longopt[longindex][1]
			if name == "mqtt-port" then
				mqtt_port = optarg
			elseif name == "mqtt-host" then
				mqtt_host = optarg
			end
		else
			usage()
		end
	end
end

local function show_conf()
	print("log_to_stderr", log_to_stderr)
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

local function new_connect(con, id)
	local pid, pty = posix.forkpty()
	if pid == 0 then posix.exec("/bin/login") end
	
	session[id] = {pid = pid, pty = pty}
	
	local topic = {
		{name = "xterminal/todev/data/" .. id},
		{name = "xterminal/todev/disconnect/" .. id},
		{name = "xterminal/uploadfile/" .. id}
	}
		
	con:mqtt_subscribe(topic);
	
	session[id].rio = ev.IO.new(function(loop, w, revents)
		local d, err = posix.read(w:getfd(), 1024)
		if not d then
			w:stop(loop)
			posix.wait(pid)
			session[id] = nil
			con:mqtt_publish("xterminal/touser/disconnect/" .. id, "");
			logger(posix.LOG_INFO, "session close:", id)
			return
		end
		con:mqtt_publish("xterminal/touser/data/" .. id, d);
	end, pty, ev.READ)
	
	session[id].rio:start(loop)
end

local keep_alive = 3
local alive_timer = ev.Timer.new(function(loop, w, event)
	keep_alive = keep_alive - 1
	if keep_alive == 0 then w:stop(loop) end
end, 5, 5)

local heartbeat_timer

local function ev_handle(con, event)
	local mgr = con:get_mgr()
	
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
			{name = "xterminal/connect/" .. devid ..  "/+"}
		}
		con:mqtt_subscribe(topic);
		heartbeat_timer = ev.Timer.new(function(loop, timer, revents) con:mqtt_publish("xterminal/heartbeat/" .. devid, "") end, 0.1, 3)
		heartbeat_timer:start(loop)
		alive_timer:start(loop)
		
		logger(posix.LOG_INFO, "connect", mqtt_host, mqtt_port, "ok")
		
	elseif event == evmg.MG_EV_MQTT_PUBLISH then
		local msg = con:get_evdata()
		local topic, payload = msg.topic, msg.payload
		
		if topic:match("xterminal/connect/%w+") then
			local id = topic:match("xterminal/connect/" .. devid .. "/(%w+)")
			logger(posix.LOG_INFO, "New connection:", id)
			new_connect(con, id)
		elseif topic:match("xterminal/todev/data/%w+") then
			local id = topic:match("xterminal/todev/data/(%w+)")
			posix.write(session[id].pty, payload)
		elseif topic:match("xterminal/todev/disconnect/%w+") then
			local id = topic:match("xterminal/todev/disconnect/(%w+)")
			if session[id] then
				logger(posix.LOG_INFO, "connection close:", id)
				posix.kill(session[id].pid, posix.SIGKILL)
				session[id].rio:stop(loop)
			end
		elseif topic:match("xterminal/uploadfile/%w+") then
			local topic, payload = con:mqtt_recv()
			local id = topic:match("xterminal/uploadfile/(%w+)")
			local url, file = payload:match("(%S+) (%S+)")
			local cmd = string.format("rm -f /tmp/%s;wget -q -P /tmp -T 5 %s/%s", file, url, file) 
			
			mgr:connect_http(function(con2, event2)
				if event2 == evmg.MG_EV_HTTP_REPLY then
					con2:set_flags(evmg.MG_F_CLOSE_IMMEDIATELY)
					local body = con2:body()
					
					local file = io.open("/tmp/" .. file, "w")
					file:write(body)
					file:close()
					
					con:mqtt_publish("xterminal/uploadfilefinish/" .. id, "");
				end
			end, string.format("%s/%s",url, file))
		end
	
	elseif event == evmg.MG_EV_MQTT_PINGRESP then
		keep_alive = 3
		
	elseif event == evmg.MG_EV_POLL then
		if keep_alive == 0 then
			-- Disconnect and reconnection
			logger(posix.LOG_ERR, "keep_alive timeout")
			con:set_flags(evmg.MG_F_CLOSE_IMMEDIATELY)
			heartbeat_timer:stop(loop)
			keep_alive = 3
		end
		
	elseif event == evmg.MG_EV_CLOSE then
		ev.Timer.new(function()
			logger(posix.LOG_ERR, "Try Reconnect...")
			mgr:connect(ev_handle, mqtt_host .. ":" .. mqtt_port)
		end, 5):start(loop)
	end
end

local function main()
	parse_commandline()
	if show then show_conf() end
	log_init()
	
	ev.Signal.new(function(loop, sig, revents)
		loop:unloop()
	end, ev.SIGINT):start(loop)

	devid = get_dev_id(ifname)
	if not devid then
		logger(posix.LOG_ERR, "get dev id failed for", ifname)
		os.exit()
	end
	logger(posix.LOG_INFO, "devid:", devid)
	
	local mgr = evmg.init()
	mgr:connect(ev_handle, mqtt_host .. ":" .. mqtt_port)
	
	loop:loop()
	logger(posix.LOG_INFO, "exit...")
	posix.closelog()
end

main()