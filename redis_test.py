import socket


with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
    s.connect(("127.0.0.1", 6379))
    s.sendall(b"set test_key test_value")
    data = s.recv(2)

print('Received', repr(data))
