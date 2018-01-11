#!/usr/bin/python3

import json
import random
import syslog
from hashlib import md5
import asyncio
import uvloop
asyncio.set_event_loop_policy(uvloop.EventLoopPolicy())
from aiohttp import web, WSMsgType

class Devices:
    devs = {}
    def add(self, ws, mac):
        self.devs[mac] = {'ws': ws, 'active': 1}
    def active(self, mac):
        self.devs[mac]['active'] += 1
    def login(self, ws, mac):
        sid = md5((mac + str(random.uniform(1, 100))).encode('utf8')).hexdigest()
        dev = self.devs.get(mac)
        if not dev:
            ws.send_str(json.dumps({'type':'login', 'err': 'Device off-line'}))
            return False
        ws.send_str(json.dumps({'type':'login', 'sid': sid}))
        dev[sid] = ws
        dev['ws'].send_str(json.dumps({'type': 'login', 'mac': mac, 'sid': sid}))
        syslog.syslog('new logged to ' + mac)
        return True
    def send_data2user(self, msg):
        mac = msg['mac']
        sid = msg['sid']
        self.devs[mac][sid].send_str(json.dumps(msg))
    def send_data2device(self, msg):
        mac = msg['mac']
        sid = msg['sid']
        self.devs[mac]['ws'].send_str(json.dumps(msg))

devices = Devices()
syslog.openlog('rttyd')

async def websocket_handler_device(request):
    ws = web.WebSocketResponse()
    await ws.prepare(request)

    mac = request.query['mac']
    devices.add(ws, mac)

    syslog.syslog('New device ' + mac)

    async for msg in ws:
        if msg.type == WSMsgType.TEXT:
            msg = json.loads(msg.data)
            typ = msg['type']
            if typ == 'ping':
                mac = msg['mac']
                devices.active(mac)
            elif typ == 'data' or typ == 'logout':
                devices.send_data2user(msg)
        elif msg.type == WSMsgType.ERROR:
            syslog.syslog('device connection closed with exception %s' % ws.exception())
    return ws

async def websocket_handler_browser(request):
    ws = web.WebSocketResponse()
    await ws.prepare(request)

    mac = request.query['mac']
    if not devices.login(ws, mac):
        ws.close()
        return ws

    async for msg in ws:
        if msg.type == WSMsgType.TEXT:
            msg = json.loads(msg.data)
            typ = msg['type']
            if typ == 'data':
                devices.send_data2device(msg)
        elif msg.type == WSMsgType.ERROR:
            syslog.syslog('browser connection closed with exception %s' % ws.exception())
    return ws

async def handle_list(request):
    return web.json_response(list(devices.devs.keys()))

app = web.Application()

app.router.add_get('/list', handle_list)
app.router.add_get('/ws/device', websocket_handler_device)
app.router.add_get('/ws/browser', websocket_handler_browser)
app.router.add_static('/', path = './www', name = 'static')

port = 8080
web.run_app(app, port = port)