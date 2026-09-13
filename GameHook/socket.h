#pragma once

#define SOCKET_SERVER "localhost"
#define SOCKET_PORT "7243"

void CloseSocket();
void ConnectSocket();
void SocketRecv(char*);
void SocketSend(const char*);
