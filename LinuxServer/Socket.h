#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include <netdb.h>
#include <netinet/in.h>

#include <pthread.h>
#include <condition_variable>
#include <stdarg.h>
#include <map>

#include <string.h>
#include <fstream>

#define PORT 3001
#define MAX_PACKET_LENGTH 512
#define THREAD_SUCCESS 0
#define SOCKET_ERROR -1
/*
setting kernel network settings for receive / send buffers
net.core.rmem_max = receive buffer / net.core.wmem_max = send buffer
sysctl net.core.rmem_max (check values)
sysctl -w net.core.rmem_max=262144 (set values)
*/
#define BUFFERSIZE (256* 1024)

class Socket;

struct ThreadParams 
{
   Socket* pSocket;
   int bClientDescriptor;
};

struct Client 
{
  int bID;
  bool State;
  std::condition_variable Cv;
};

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
typedef unsigned char BYTE;
typedef std::map<unsigned long, Client*> ClientMap;

struct Packet {
   int bSize;
   BYTE byType;
   char data[MAX_PACKET_LENGTH];
};

class Socket {
private:
  int m_bSocket;
  bool m_canRun;

  ClientMap m_mClientMap;
  std::mutex m_mxClientMap;

  void RecvLoop(int bClientDescriptor);
  void ReadInFile(int size, char* name);
  char* WritePacket(char* packet, va_list va);
  char* ReadPacket(char* packet, const char* format, ...);
  void SendFile(const char* name);
  bool SendPacket(int bClientDescriptor, BYTE byType, ...);
  void Process(Packet packet);

  void AddClient(unsigned long id, Client* pClient);
  void RemoveClient(unsigned long id);

public:
   Socket();
   ~Socket();
   bool Init();
   void AcceptLoop();
   static void* ThreadStart(void* lParam);
};