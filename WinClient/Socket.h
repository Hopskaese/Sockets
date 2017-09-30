#pragma once
#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <iostream>
#include <ws2tcpip.h>
#include <winsock2.h>
#include <commdlg.h>
#include <condition_variable>
#include "GlobalFuncs.h"

#pragma comment (lib, "Ws2_32.lib")
#pragma comment (lib, "Mswsock.lib")
#pragma comment (lib, "AdvApi32.lib")

#define PORT "3001"
#define SERVER "192.168.163.129"
#define MAX_PACKET_LENGTH 512
#define BUFFERSIZE (256* 1024)

enum C2S_PROTOCOL
{
	C2S_DOCUMENT,
	C2S_MESSAGE,
	C2S_STATE
};

enum S2C_PROTOCOL
{
	S2C_DOCUMENT,
	S2C_MESSAGE,
	S2C_STATE,
	S2C_DISCONNECT
};

typedef char PACKETBUFFER[MAX_PACKET_LENGTH];

struct Packet {
	int bSize;
	BYTE byType;
	char data[MAX_PACKET_LENGTH];
};

class Socket {
private:
	SOCKET m_pConnectSocket;
	std::condition_variable m_Cv;
	std::mutex m_Mtx;
	bool m_ServerState;

	char* WritePacket(char* packet, va_list va);
	char* ReadPacket(char* packet, const char* format, ...);
	void ReadInFile(int size, char* name);
	void SendFile(std::string path);
	void Process(Packet packet);
	bool SendPacket(BYTE bType, ...);
	void PrintErrorMsg();
public:
	Socket();
	~Socket() {};
	bool Init();
	void RecvLoop();
	void InteractionLoop();
	static DWORD ThreadStart(LPVOID lParam);
};
