import socket

host = 'localhost'
port = 8303
s = input()

with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
    sock.connect((host, port))
    print('Connected')
    sock.sendall(bytes(s + '\n', 'utf-8'))
    print('Sent data')
    recv = str(sock.recv(1024), 'utf-8')
print(recv)