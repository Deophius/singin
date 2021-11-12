#!/usr/local/bin/python3
# Use this on seewo or any computer that's reasonable to access
# This file connects to host:port, which signin.py is listening upon.
# Once accept() in that file returns, auto-DK starts.
# Otherwise, it's kept at bay.

import socket
import json
import sys
from time import gmtime
host = '192.168.25.77'
port = 8512
soc = socket.socket()
soc.connect((host, port))

def process_json(s):
    # print(s)
    d = json.loads(s)
    ts = gmtime(d['time'])
    if d['type'] == 'info':
        print('{:0>2}:{:0>2}:{:0>2}'.format((ts.tm_hour + 8) % 24, ts.tm_min, ts.tm_sec), d['info'])
    elif d['type'] == 'error':
        print('{:0>2}:{:0>2}:{:0>2}'.format((ts.tm_hour + 8) % 24, ts.tm_min, ts.tm_sec), d['error'], file = sys.stderr)
    else:
        print('Received TERMINAL signal')
        soc.close()
        sys.exit(0)

while True:
    recv = str(soc.recv(1000)).strip('b\'').strip('+').split('+')
    # print(repr(recv))
    for s in recv:
        process_json(s)

