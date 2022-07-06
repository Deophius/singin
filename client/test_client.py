from socket import *
import json

addr = (input('Please input host: '), 8303)
s = socket(AF_INET, SOCK_DGRAM)
s.sendto(input().encode('utf-8'), addr)
print(s.recv(1024))
