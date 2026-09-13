#include "socket.h"
#include "logging.h"

#include <winsock2.h>
#include <ws2tcpip.h>

static SOCKET g_socket = INVALID_SOCKET;

void CloseSocket()
{
    if (g_socket == INVALID_SOCKET)
        return;

    shutdown(g_socket, SD_BOTH);
    closesocket(g_socket);
    g_socket = INVALID_SOCKET;
    WSACleanup();
}

void ConnectSocket()
{
    if (g_socket != INVALID_SOCKET)
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
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_UDP;

    iResult = getaddrinfo(SOCKET_SERVER, SOCKET_PORT, &hints, &result);
    if (iResult != 0)
    {
        DebugLogF("getaddrinfo failed: %d\n", iResult);
        WSACleanup();
        return;
    }

    g_socket = socket(result->ai_family, result->ai_socktype,  result->ai_protocol);
    if (g_socket == INVALID_SOCKET)
    {
        DebugLogF("Error at socket(): %ld\n", WSAGetLastError());
        freeaddrinfo(result);
        WSACleanup();
        return;
    }

    iResult = connect( g_socket, result->ai_addr, (int)result->ai_addrlen);
    if (iResult == SOCKET_ERROR)
    {
        DebugLogF("Error at connect(): %ld\n", WSAGetLastError());
        CloseSocket();
    }

    freeaddrinfo(result);
    if (g_socket == INVALID_SOCKET)
    {
        DebugLogF("Unable to connect to server!\n");
        WSACleanup();
        return;
    }
}

void SocketRecv(char* buffer)
{
    if (g_socket == INVALID_SOCKET)
        return;

    int iResult = recv(g_socket, buffer, sizeof(buffer), 0);
    if (iResult == 0)
    {
        DebugLog("Connection closed by server.\n");
        CloseSocket();
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
    if (g_socket == INVALID_SOCKET)
        return;

    int iResult = send(g_socket, sendbuf, (int)strlen(sendbuf), 0);
    if (iResult == SOCKET_ERROR)
    {
        DebugLogF("send failed: %d\n", WSAGetLastError());
        CloseSocket();
    }
}
