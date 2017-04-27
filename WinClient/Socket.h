#pragma once
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>
#include <iostream>

#include "Commands.h"

#pragma comment (lib, "Ws2_32.lib")
#pragma comment (lib, "Mswsock.lib")
#pragma comment (lib, "AdvApi32.lib")

#define PORT "3001"
#define SERVER "192.168.178.184"
#define MAX_PACKET_LENGTH 512

typedef char PACKETBUFFER[MAX_PACKET_LENGTH];

struct Packet {
	int bSize;
	BYTE byType;
	char data[MAX_PACKET_LENGTH];
};

class Socket {
	SOCKET m_ConnectSocket;

public:
	Socket() {};
	char* WritePacket(char* packet, va_list va);
	char* ReadPacket(char* packet, const char* format, ...);
	void ReadInFile(int size, char* name);
	void SendFile(const char* name);
	bool Init();
	void RecvLoop();
	void Process(Packet packet);
	bool SendPacket(BYTE bType, ...);
	static DWORD ThreadStart(LPVOID lParam);
};

