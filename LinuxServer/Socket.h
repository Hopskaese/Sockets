#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <netdb.h>
#include <netinet/in.h>

#include <pthread.h>
#include <stdarg.h>

#include <string.h>
#include <fstream>

#include "Commands.h"

#define PORT 3001
#define MAX_PACKET_LENGTH 512

typedef char PACKETBUFFER[MAX_PACKET_LENGTH];
typedef unsigned char BYTE;

struct Packet {
   int bSize;
   BYTE byType;
   char data[MAX_PACKET_LENGTH];
};

class Socket {
  int m_bClientSock;

public:
   Socket() {};
   bool Init();
   void RecvLoop();
   void ReadInFile(int size, char* name);
   char* WritePacket(char* packet, va_list va);
   char* ReadPacket(char* packet, const char* format, ...);
   static void* ThreadStart(void* lParam);
   void SendFile(const char* name);
   bool SendPacket(BYTE byType, ...);
   void Process(Packet packet);
};