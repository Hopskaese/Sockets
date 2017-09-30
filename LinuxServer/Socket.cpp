#include "Socket.h"

Socket::Socket()
{
   m_bSocket = 0;
   m_canRun = true;
}

Socket::~Socket()
{
   m_canRun = false;
   close(m_bSocket);

   for (auto const &client : m_mClientMap)
      SendPacket(client.second->bID, S2C_DISCONNECT, "");
}

bool Socket::Init()
{
   struct sockaddr_in serv_addr;
   struct hostent *server;
   PACKETBUFFER buffer;

   if ((m_bSocket = socket(AF_INET, SOCK_STREAM, 0)) == SOCKET_ERROR)
   {
      perror("Error on calling socket()");
      return false;
   }

   memset(&serv_addr, 0, sizeof(serv_addr));

   serv_addr.sin_family = AF_INET;
   serv_addr.sin_addr.s_addr = INADDR_ANY;
   serv_addr.sin_port = htons(PORT);

   if (bind(m_bSocket, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) == SOCKET_ERROR) 
   {
      perror("Error on bind");
      return false;
   }

   if (listen(m_bSocket ,5) == SOCKET_ERROR)
   {
      perror ("Error on listen");
      return false;
   }

   return true;
}

void Socket::Process(Packet packet)
{
   switch (packet.byType)
   {
      case C2S_MESSAGE:
      {
         char* pMessage = NULL;
         ReadPacket(packet.data, "s", &pMessage);
         printf("Nachricht: %s\n", pMessage);
      }
      break;
      case C2S_DOCUMENT:
      {
         int bSize = 0;
         char* pName = NULL;
         ReadPacket(packet.data, "bs", &bSize, &pName);
         ReadInFile(bSize, pName);
      }
      break;
      case C2S_STATE:
      {
         Client* pClient = m_mClientMap[pthread_self()];
         ReadPacket(packet.data, "b", &pClient->State);

         pClient->Cv.notify_all();
      }
      break;
   }
}

void Socket::AcceptLoop() 
{
   struct sockaddr_in cli_addr;
   socklen_t clilen = sizeof(cli_addr);
   int client = 0;
   while (true)
   {
      if((client = accept(m_bSocket, (struct sockaddr *)&cli_addr, &clilen)) == SOCKET_ERROR)
      {
         perror("Error on accpet");
         continue;
      }

      pthread_t t;
      ThreadParams tp;
      tp.pSocket = this;
      tp.bClientDescriptor = client;

      if (pthread_create(&t, NULL, &Socket::ThreadStart, (void*)&tp) != THREAD_SUCCESS)
      {
         printf("Error opening thread\n");
         close(client);
         continue;
      }
      printf("Client connected, ID: %lu\n", t);
   }
}

void Socket::RecvLoop(int bClientDescriptor)
{
   int bLen = 0;

   Client* client = new Client();
   client->bID = bClientDescriptor;
   client->State = false;
   AddClient(pthread_self(), client);

   while (m_canRun)
   {
      PACKETBUFFER buffer;
      memset(&buffer, 0, sizeof(PACKETBUFFER));

      if((bLen = recv(bClientDescriptor, buffer, sizeof(PACKETBUFFER), 0)) == SOCKET_ERROR)
      {
         perror("Error on recv");
         close(bClientDescriptor);
         RemoveClient(pthread_self());
         break;
      }

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

   pthread_detach(pthread_self());
}

bool Socket::SendPacket(int bClientDescriptor, BYTE byType, ...)
{
   Packet packet;
   memset(&packet, 0, sizeof(Packet));

   packet.byType = byType;

   va_list va;
   va_start(va, byType);

   char* end = WritePacket(packet.data, va);
   va_end(va);

   packet.bSize = end - (char*)&packet;

   if (send(bClientDescriptor, (char*)&packet, packet.bSize, 0) == SOCKET_ERROR)
   {
      perror("Error on sending Packet");
      close(bClientDescriptor);
      RemoveClient(pthread_self());
      return false;
   }
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
         int size = va_arg(va, int);
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
   ThreadParams* pTp = (ThreadParams*)lParam;
   pTp->pSocket->RecvLoop(pTp->bClientDescriptor);
}

void Socket::ReadInFile(int size, char* name)
{
   printf("About to receive file of Size: %d Bytes\n", size);
   Client* pClient = m_mClientMap[pthread_self()];
   char Buffer[BUFFERSIZE];
   FILE* pFile = NULL;
   if ((pFile = fopen(name, "wb")) == NULL)
   {
      perror("Error opening file\n");
      SendPacket(pClient->bID, S2C_STATE, "b", false);
      return;
   }

   int bRecvCount = 0;
   int bReceived = 0;
   SendPacket(pClient->bID, S2C_STATE, "b", true);
   do
   {
      if ((bRecvCount = recv(pClient->bID, Buffer, BUFFERSIZE, 0)) == SOCKET_ERROR)
      {
         perror("Error reading in file");
         close(pClient->bID);
         RemoveClient(pthread_self());
         delete pClient;
         fclose(pFile);
         return;
      }
      fwrite(Buffer, 1, bRecvCount, pFile);
      bReceived += bRecvCount;
      printf("Received: %d\n", bReceived);
   } while (bReceived < size);
   printf("File successfully received\n");
   fclose(pFile);
}

void Socket::SendFile(const char* name)
{
   Client* pClient = m_mClientMap[pthread_self()];
   FILE * pFile = fopen(name, "rb");
   if (pFile == NULL)
   {
      perror("Error opening the file to be send");
      return;
   }
   fseek(pFile, 0, SEEK_END);
   int bLen = ftell(pFile);
   SendPacket(pClient->bID, S2C_DOCUMENT, "bs", bLen, name);

   char Buffer[BUFFERSIZE];
   rewind(pFile);

   int bOffset = 0;
   int bSentCount = 0;
   int bReadCount = 0;

   std::mutex Mtx;
   std::unique_lock<std::mutex>lk(Mtx);
   pClient->Cv.wait(lk);

   if (pClient->State)
   {
      do
      {
         bOffset = 0;
         bReadCount = fread(Buffer, 1, BUFFERSIZE, pFile);
         while (bOffset < bReadCount)
         {
            if ((bSentCount = send(pClient->bID, Buffer, bReadCount - bOffset, 0)) == SOCKET_ERROR)
            {
               perror("Error on sending file");
               close(pClient->bID);
               RemoveClient(pthread_self());
               delete pClient;
               fclose(pFile);
               return;
            }
            bOffset += bSentCount;
         }
      } while (bReadCount == BUFFERSIZE);
   }
   else
   {
      fprintf(stderr, "A clientside error occured. Cannot send file\n");
   }
   fclose(pFile);
}

void Socket::AddClient(unsigned long id, Client* pClient)
{
   m_mxClientMap.lock();

   if (m_mClientMap.find(id) == m_mClientMap.end())
      m_mClientMap[id] = pClient;

   m_mxClientMap.unlock();
}

void Socket::RemoveClient(unsigned long id)
{
   m_mxClientMap.lock();

   ClientMap::iterator it = m_mClientMap.find(id);
   if (it != m_mClientMap.end())
      m_mClientMap.erase(it);
   
   m_mxClientMap.unlock();
}
