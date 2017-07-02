#!/usr/bin/lua

local ev = require("ev")
local evmg = require("evmongoose")
local posix = require('posix')
local cjson = require("cjson")
local syslog = evmg.syslog

local loop = ev.Loop.default
local mgr = evmg.init()

local ARGV = arg
local show = false
local log_to_stderr = false
local ifname = "eth0"
local mqtt_host = "localhost"
local mqtt_port = "1883"
local keepalive = 0
local devid = nil
local session = {}

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
	local long = {
		{"help",  "none", 'h'},
		{"mqtt-port", "required", "0"},
		{"mqtt-host", "required", "0"}
	}
	
	for r, optarg, optind, longindex in posix.getopt(ARGV, "hsdi:", long) do
		if r == '?' or r == "h" then
			usage()
		end
		
		if r == "d" then
			log_to_stderr = true
		elseif r == "i" then
			ifname = optarg
		elseif r == "s" then
			show = true
		elseif r == "0" then
			local name = long[longindex + 1][1]
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

local function new_connect(nc, id)
	local pid, pty = evmg.forkpty()
	if pid == 0 then posix.exec ("/bin/login", {}) end
	
	session[id] = {pid = pid, pty = pty}
	
	mgr:mqtt_subscribe(nc, "xterminal/todev/data/" .. id);
	mgr:mqtt_subscribe(nc, "xterminal/todev/disconnect/" .. id);
	mgr:mqtt_subscribe(nc, "xterminal/uploadfile/" .. id);
	
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
		if msg.code ~= evmg.MG_EV_MQTT_CONNACK_ACCEPTED then
			logger(syslog.LOG_ERR, "Got mqtt connection error:", msg.code, msg.reason)
			return
		end
		
		mgr:mqtt_subscribe(nc, "xterminal/connect/" .. devid ..  "/+");
		
		ev.Timer.new(function(loop, timer, revents)
			mgr:mqtt_publish(nc, "xterminal/heartbeat/" .. devid, "")
		end, 0.1, 3):start(loop)
		
		ev.Timer.new(function(loop, timer, revents)
			mgr:mqtt_ping(nc)
		end, 1, 10):start(loop)
		
		logger(syslog.LOG_INFO, "connect", mqtt_host, mqtt_port, "ok")
		
	elseif event == evmg.MG_EV_MQTT_PUBLISH then
		local topic = msg.topic
		if topic:match("xterminal/connect/%w+") then
			local id = topic:match("xterminal/connect/" .. devid .. "/(%w+)")
			logger(syslog.LOG_INFO, "New connection:", id)
			new_connect(nc, id)
		elseif topic:match("xterminal/todev/data/%w+") then
			local id = topic:match("xterminal/todev/data/(%w+)")
			posix.write(session[id].pty, msg.payload)
		elseif topic:match("xterminal/todev/disconnect/%w+") then
			local id = topic:match("xterminal/todev/disconnect/(%w+)")
			if session[id] then
				logger(syslog.LOG_INFO, "connection close:", id)
				posix.kill(session[id].pid)
				session[id].rio:stop(loop)
			end
		elseif topic:match("xterminal/uploadfile/%w+") then
			local id = topic:match("xterminal/uploadfile/(%w+)")
			local url, file = msg.payload:match("(%S+) (%S+)")
			local cmd = string.format("rm -f /tmp/%s;wget -q -P /tmp -T 5 %s/%s", file, url, file) 
			print(cmd)
			os.execute(cmd)
			mgr:mqtt_publish(nc, "xterminal/uploadfilefinish/" .. id, "");
		end
	elseif event == evmg.MG_EV_MQTT_PINGRESP then
		keepalive = 3
	end
end

parse_commandline()
if show then show_conf() end

log_init()

devid = get_dev_id(ifname)
if not devid then
	logger(syslog.LOG_ERR, "get dev id failed for", ifname)
	os.exit()
end

logger(syslog.LOG_INFO, "devid:", devid)

mgr:connect(mqtt_host .. ":" .. mqtt_port, ev_handle)

ev.Signal.new(function(loop, sig, revents)
	loop:unloop()
end, ev.SIGINT):start(loop)

logger(syslog.LOG_INFO, "start...")

ev.Timer.new(function(loop, timer, revents)
	if keepalive == 0 then
		logger(syslog.LOG_INFO, "re connect......")
		mgr:connect(mqtt_host .. ":" .. mqtt_port, ev_handle)
	else
		keepalive = keepalive - 1
	end
end, 10, 10):start(loop)
		
loop:loop()

mgr:destroy()

logger(syslog.LOG_INFO, "exit...")

syslog.closelog()