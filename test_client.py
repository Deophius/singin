import socket

# host = socket.gethostbyname(socket.gethostname())
host = '255.255.255.255'
port = 8303
s = input()

with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
    sock.settimeout(10)
    sock.sendto(bytes(s + '\n', 'utf-8'), (host, port))
    print('Sent data')
    try:
        recv = str(sock.recv(1024), 'utf-8')
    except socket.timeout:
        print('Unable to receive any data!')
        import sys
        sys.exit(0)
print(recv)