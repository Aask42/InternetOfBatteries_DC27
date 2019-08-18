import socket

HOST = ''	# Symbolic name, meaning all available interfaces
PORT = 9000	# Arbitrary non-privileged port

s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
print('Socket created')

#Bind socket to local host and port
try:
	s.bind((HOST, PORT))
except socket.error as msg:
	print('Bind failed. Error Code : ' + str(msg[0]) + ' Message ' + msg[1])
	sys.exit()
	
print('Socket bind complete')

#Start listening on socket
s.listen(10)
print('Socket now listening')

#now keep talking with the client
while 1:
    try:
        #wait to accept a connection - blocking call
        conn, addr = s.accept()
        print('Connected with ' + addr[0] + ':' + str(addr[1]))
        Header = """HTTP/1.1 200 OK
        Connection: close
Content-Type: text/html
Server: local
Content-Length: %d

""".replace("\n", "\r\n")

        Data = open("test.html","rb").read()
        HData = Header % len(Data)
        Data = HData.encode("utf-8") + Data
        conn.sendall(Data)

    except Exception as ex:
        print(ex)
        break

s.close()