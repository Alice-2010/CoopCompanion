#pragma once

#define SOCKET_SERVER "127.0.0.1"
#define SOCKET_PORT "7243"

void SocketClose();
void SocketConnect();
void SocketRecv(char*);
void SocketSend(const char*);
bool IsSocketConnected();
