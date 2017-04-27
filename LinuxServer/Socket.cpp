#include "Socket.h"

#define PORT 3001

bool Socket::Init()
{
   int sockfd, n;
   struct sockaddr_in serv_addr, cli_addr;
   socklen_t clilen = sizeof(cli_addr);
   struct hostent *server;
   PACKETBUFFER buffer;

   sockfd = socket(AF_INET, SOCK_STREAM, 0);

   if (sockfd < 0)
   {
      printf("Error opening socket \n");
      return false;
   }

   bzero((char *) &serv_addr, sizeof(serv_addr));

   serv_addr.sin_family = AF_INET;
   serv_addr.sin_addr.s_addr = INADDR_ANY;
   serv_addr.sin_port = htons(PORT);

   if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
      printf("Error on binding \n");
      return false;
   }

   listen(sockfd,5);

   m_bClientSock = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);

   if (m_bClientSock < 0) {
      printf("Error on accept \n");
      return false;
   }

   return true;
   
}

void Socket::Process(Packet packet)
{
   switch (packet.byType)
   {
      case MESSAGE:
         char* pMessage;
         ReadPacket(packet.data, "s", &pMessage);
         printf("Nachricht: %s\n", pMessage);
         break;
      case TEST:
         char* pTestMsg;
         int bNumber;
         ReadPacket(packet.data, "sb", &pTestMsg, &bNumber);
         printf("Testmsg %s\n", pTestMsg);
         printf("Testnumber %d\n", bNumber);
         break;
      case DOCUMENT:
         int bSize = 0;
         char* pName = NULL;
         ReadPacket(packet.data, "bs", &bSize, &pName);
         ReadInFile(bSize, pName);
         break;
   }
}

void Socket::RecvLoop()
{
   while (true)
   {
      PACKETBUFFER buffer;
      memset(&buffer, 0, sizeof(PACKETBUFFER));

      int bLen = recv(m_bClientSock, buffer, sizeof(PACKETBUFFER), 0);
      if (bLen <= 0)
      {
         printf("Socket-recv error}\n");
         break;
      }

      printf("Got a package\n");

      if (bLen > MAX_PACKET_LENGTH) continue;

      char* p = buffer;
      while (bLen > 0 && bLen >= *(int*)p)
      {
         Packet packet;
         memset(&packet, 0, sizeof(Packet));
         //copying packet into new Packet
         memcpy(&packet, p, *(int*)p);

         Process(packet);

         // length left to read is lessen by the last packet size
         bLen -= *(int*)p;
         // pointer increased to next packet
         p += *(int*)p;
      }
   }
}

bool Socket::SendPacket(BYTE byType, ...)
{
   printf("sending packets\n");
   Packet packet;
   memset(&packet, 0, sizeof(Packet));

   packet.byType = byType;

   va_list va;
   va_start(va, byType);

   char* end = WritePacket(packet.data, va);
   va_end(va);

   packet.bSize = end - (char*)&packet;

   int bSentCount = send(m_bClientSock, (char*)&packet, packet.bSize, 0);
   if (bSentCount <= 0)
      return false;

   return true;
}

char* Socket::ReadPacket(char* packet, const char* format, ...)
{
   va_list va;
   va_start(va, format);

   char byArgNum = strlen(format);
   for (char i = 0; i < byArgNum; i++)
   {
      switch (format[i])
      {
      case 'b':
      {
         *va_arg(va, int*) = *(int *)packet;
         packet += sizeof(int);
      }
      break;
      case 's':
      {
         char *text = *va_arg(va, char**) = packet;
         while (*packet++) {}
      }
      break;
      case 0:
      default:
         return packet;
      }
   }
   va_end(va);
   return packet;
}

char* Socket::WritePacket(char* packet, va_list va)
{
   char* format = va_arg(va, char*);

   printf("Format: %s\n", format);

   for (; ; ) {
      switch (*format++) {
      case 'b':
      case 'B':
      {
         *(int *)packet = va_arg(va, int);
         packet += sizeof(int);

      }
      break;
      case 's':
      case 'S':
      {
         char *str = va_arg(va, char *);
         if (str == 0 || *str == 0) {
            *packet++ = 0;
            break;
         }
         for (; (*packet++ = *str++) != 0; )
            ;
      }
      break;
      case 'm':
      case 'M':
      {
         char *ptr = va_arg(va, char *);
         int   size = va_arg(va, int);
         memcpy(packet, ptr, size);
         packet += size;
      }
      break;
      case 0:
      default:
         return packet;
      }
   }
   return packet;
}

void* Socket::ThreadStart(void* lParam)
{
   Socket* pSocket = (Socket*)lParam;
   pSocket->RecvLoop();

   pthread_exit(NULL);
}

void Socket::ReadInFile(int size, char* name)
{
   int bOffset = 0;
   int bRecvCount = 0;
   char* pBuffer = new char[size];
   FILE* pFile;

   while (bOffset < size)
   {
      bRecvCount = recv(m_bClientSock, pBuffer + bOffset, size - bOffset, 0);
      if (bRecvCount <= 0) 
         return;
   
      bOffset += bRecvCount;
   }

   pFile = fopen(name, "wb");
   fwrite(pBuffer, 1, size, pFile);
   fclose(pFile);
}

void Socket::SendFile(const char* name)
{
   FILE * pFile = fopen(name, "rb");
   if (pFile == NULL)
      return;
   fseek(pFile, 0, SEEK_END);
   int bLen = ftell(pFile);
   fseek(pFile, 0, SEEK_SET);
   char* pBuffer = new char[bLen];
   fread(pBuffer, 1, bLen, pFile);
   fclose(pFile);

   SendPacket(DOCUMENT, "bs", bLen, name);

   int bOffset = 0;
   int bSentCount = 0;

   while (bOffset < bLen)
   {
      bSentCount = send(m_bClientSock, pBuffer + bOffset, bLen - bOffset, 0);
      if (bSentCount <= 0)
         return;

      bOffset += bSentCount;
   }
}
