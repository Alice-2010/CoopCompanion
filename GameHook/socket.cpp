#include "socket.h"
#include "logging.h"

#include <winsock2.h>
#include <ws2tcpip.h>

static SOCKET g_socket = INVALID_SOCKET;

void SocketClose()
{
    if (IsSocketConnected()) {
        SocketSend("{\"type\":\"disconnect\"}");
        shutdown(g_socket, SD_BOTH);
        closesocket(g_socket);
        g_socket = INVALID_SOCKET;
    }
    WSACleanup();
    DebugLog("Disconnected from server.\n");
}

void SocketConnect()
{
    if (IsSocketConnected())
        return;

    WSADATA wsaData;

    int iResult = WSAStartup(MAKEWORD(2,2), &wsaData);
    if (iResult != 0)
    {
        DebugLogF("WSAStartup failed: %d\n", iResult);
        return;
    }

    struct addrinfo *result = NULL, hints;

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;

    iResult = getaddrinfo(SOCKET_SERVER, SOCKET_PORT, &hints, &result);
    if (iResult != 0)
    {
        DebugLogF("getaddrinfo failed: %d\n", iResult);
        SocketClose();
        return;
    }

    g_socket = socket(result->ai_family, result->ai_socktype,  result->ai_protocol);
    if (g_socket == INVALID_SOCKET)
    {
        DebugLogF("Error at socket(): %ld\n", WSAGetLastError());
        freeaddrinfo(result);
        SocketClose();
        return;
    }

    iResult = connect( g_socket, result->ai_addr, (int)result->ai_addrlen);
    freeaddrinfo(result);
    if (iResult == SOCKET_ERROR)
    {
        DebugLogF("Error at connect(): %ld\n", WSAGetLastError());
        SocketClose();
        return;
    }

    if (g_socket == INVALID_SOCKET)
    {
        DebugLog("Unable to connect to server!\n");
        SocketClose();
        return;
    }
    SocketSend("{\"type\":\"connect\"}");
    DebugLog("Connected to server.\n");
}

void SocketRecv(char* buffer)
{
    if (!IsSocketConnected())
        return;

    int iResult = recv(g_socket, buffer, sizeof(buffer), 0);
    if (iResult == 0)
    {
        DebugLog("Connection closed by server.\n");
        SocketClose();
        return;
    }
    else if (iResult == SOCKET_ERROR)
    {
        DebugLogF("recv failed: %d\n", WSAGetLastError());
        return;
    }
}

void SocketSend(const char* sendbuf)
{
    if (!IsSocketConnected())
        return;

    int iResult = send(g_socket, sendbuf, (int)strlen(sendbuf), 0);
    if (iResult == SOCKET_ERROR)
    {
        DebugLogF("send failed: %d\n", WSAGetLastError());
        SocketClose();
    }
}

bool IsSocketConnected()
{
    return g_socket != INVALID_SOCKET;
}
