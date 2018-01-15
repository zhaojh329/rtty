#!/usr/bin/python3

import sys
import json
import random
import syslog
import getopt
from hashlib import md5
import asyncio
import uvloop
asyncio.set_event_loop_policy(uvloop.EventLoopPolicy())
from aiohttp import web, WSMsgType

class Devices:
    devs = {}

    def add(self, ws, did):
        if self.devs.get(did):
            return False
        self.devs[did] = {'ws': ws, 'active': 3}
        return True

    def active(self, did):
        self.devs[did]['active'] = 3

    def flush(self):
        for did in list(self.devs):
            self.devs[did]['active'] -= 1
            if self.devs[did]['active'] == 0:
                self.devs[did]['ws'].close()
                del self.devs[did]

    def login(self, ws, did):
        sid = md5((did + str(random.uniform(1, 100))).encode('utf8')).hexdigest()
        dev = self.devs.get(did)
        if not dev:
            ws.send_str(json.dumps({'type':'login', 'err': 'Device off-line'}))
            return False
        ws.send_str(json.dumps({'type':'login', 'sid': sid}))
        dev[sid] = ws
        dev['ws'].send_str(json.dumps({'type': 'login', 'did': did, 'sid': sid}))
        syslog.syslog('new logged to ' + did)
        return sid

    def logout(self, did, sid):
        dev = self.devs.get(did)
        if dev and dev.get(sid):
            del dev[sid]
            dev['ws'].send_str(json.dumps({'type': 'logout', 'did': did, 'sid': sid}))

    def send_data2user(self, did, msg):
        sid = msg['sid']
        self.devs[did][sid].send_str(json.dumps(msg))

    def send_data2device(self, msg):
        did = msg['did']
        sid = msg['sid']
        self.devs[did]['ws'].send_str(json.dumps(msg))

devices = Devices()
syslog.openlog('rttyd')

async def websocket_handler_device(request):
    ws = web.WebSocketResponse()
    await ws.prepare(request)

    did = request.query['did']
    if not devices.add(ws, did):
        # Fix me
        ws.send_str(json.dumps({'type': 'add', 'err': 'ID conflicts'}))
        return ws

    syslog.syslog('New device:' + did)

    async for msg in ws:
        if msg.type == WSMsgType.TEXT:
            msg = json.loads(msg.data)
            typ = msg['type']
            if typ == 'ping':
                devices.active(did)
                ws.send_str(json.dumps({'type': 'pong'}))
            elif typ == 'data' or typ == 'logout':
                devices.send_data2user(did, msg)
        elif msg.type == WSMsgType.ERROR:
            syslog.syslog('device connection closed with exception %s' % ws.exception())
    return ws

async def websocket_handler_browser(request):
    ws = web.WebSocketResponse()
    await ws.prepare(request)

    did = request.query['did']
    sid = devices.login(ws, did)
    if not sid:
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
    devices.logout(id, sid)
    return ws

async def handle_list(request):
    return web.json_response(list(devices.devs.keys()))

async def handle_root(request):
    return web.Response(status = 302, headers = {'location': '/rtty.html'})

port = 5912
document = './www'
opts, args = getopt.getopt(sys.argv[1:], "p:d:")
for op, value in opts:
    if op == '-p':
        port = int(value)
    elif op == '-d':
        document = value


app = web.Application()

app.router.add_get('/list', handle_list)
app.router.add_get('/ws/device', websocket_handler_device)
app.router.add_get('/ws/browser', websocket_handler_browser)
app.router.add_get('/', handle_root)
app.router.add_static('/', path = document, name = 'static')

def keepalive(loop):
    devices.flush()
    loop.call_later(5, keepalive, loop)

loop = asyncio.get_event_loop()
loop.call_later(5, keepalive, loop)

web.run_app(app, port = port)
