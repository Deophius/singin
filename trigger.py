#!/usr/local/bin/python3
# Use this on seewo or any computer that's reasonable to access
# This file connects to host:port, which signin.py is listening upon.
# Once accept() in that file returns, auto-DK starts.
# Otherwise, it's kept at bay.

import socket
host = '192.168.5.143'
port = 8512
soc = socket.socket()
soc.connect((host, port))
