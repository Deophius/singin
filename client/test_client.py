from socket import *

addr = ('127.0.0.1', 8303)
s = socket(AF_INET, SOCK_DGRAM)
s.sendto(input().encode('utf-8'), addr)
print(s.recv(1024).decode())
