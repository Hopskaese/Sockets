#include "Socket.h"

bool Socket::Init()
{
	WSADATA wsaData;
	struct addrinfo *result = NULL,
		*ptr = NULL,
		hints;
	int iResult;

	iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult != 0) {
		printf("WSAStartup failed with error: %d\n", iResult);
		return false;
	}

	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	iResult = getaddrinfo(SERVER, PORT, &hints, &result);
	if (iResult != 0) {
		printf("getaddrinfo failed with error: %d\n", iResult);
		WSACleanup();
		return false;
	}

	for (ptr = result; ptr != NULL; ptr = ptr->ai_next) {
		m_ConnectSocket = socket(ptr->ai_family, ptr->ai_socktype,
			ptr->ai_protocol);
		if (m_ConnectSocket == INVALID_SOCKET) {
			printf("socket failed with error: %ld\n", WSAGetLastError());
			WSACleanup();
			return false;
		}

		iResult = connect(m_ConnectSocket, ptr->ai_addr, (int)ptr->ai_addrlen);
		if (iResult == SOCKET_ERROR) {
			closesocket(m_ConnectSocket);
			m_ConnectSocket = INVALID_SOCKET;
			continue;
		}
		break;
	}
	std::cout << "Connected!" << std::endl;
	return true;
}

char* Socket::WritePacket(char* packet, va_list va)
{
	char* format = va_arg(va, char*);

	for (; ; ) {
		switch (*format++) {
		case 'b':
		case 'B':
			*(int *)packet = va_arg(va, int);
			packet += sizeof(int);
		break;
		case 's':
		case 'S':
		{
			char *str = va_arg(va, char *);
			printf("*str: %s\n", str);
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
			int	size = va_arg(va, int);
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
			*va_arg(va, int*) = *(int *)packet;
			packet += sizeof(int);
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

	int bSentCount = send(m_ConnectSocket, (char*)&packet, packet.bSize, 0);
	if (bSentCount <= 0)
		return false;

	return true;
}

void Socket::RecvLoop()
{
	while (true)
	{
		PACKETBUFFER pBuffer;
		memset(&pBuffer, 0, sizeof(PACKETBUFFER));

		int bLen = recv(m_ConnectSocket, pBuffer, sizeof(PACKETBUFFER), 0);
		if (bLen <= 0)
		{
			std::cout << "Socket-recv error" << std::endl;
			closesocket(m_ConnectSocket);
			WSACleanup;

			break;
		}
		printf("Got a package\n");

		if (bLen > MAX_PACKET_LENGTH) continue;

		char* p = pBuffer;
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

void Socket::SendFile(const char* name)
{
	std::cout << "Name: " << name << std::endl;
	FILE *pFile = fopen(name, "rb");
	if (pFile == NULL)
	{
		printf("pfile is null\n");
		return;
	}

	fseek(pFile, 0, SEEK_END);
	int bLen = ftell(pFile);
	fseek(pFile, 0, SEEK_SET);
	char* p_Buffer = new char[bLen];
	fread(p_Buffer, 1, bLen, pFile);
	fclose(pFile);

	SendPacket(DOCUMENT, "bs", bLen, name);
	printf("Len sent: %d \n", bLen);

	int bOffset = 0;
	int bSentCount = 0;

	while (bOffset < bLen)
	{
		bSentCount = send(m_ConnectSocket, p_Buffer + bOffset, bLen - bOffset, 0);
		if (bSentCount <= 0)
			return;
		printf("Sending file data\n");
		bOffset += bSentCount;
	}
}

void Socket::ReadInFile(int size, char* name)
{
	int bOffset = 0;
	int bRecvCount = 0;
	char* pBuffer = new char[size];
	FILE* pFile;

	while (bOffset < size)
	{
		bRecvCount = recv(m_ConnectSocket, pBuffer + bOffset, size - bOffset, 0);
		if (bRecvCount <= 0)
			return;

		bOffset += bRecvCount;
	}

	pFile = fopen(name, "wb");
	fwrite(pBuffer, 1, size, pFile);
	fclose(pFile);
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

DWORD Socket::ThreadStart(LPVOID lParam)
{
	Socket *pSocket = (Socket*)lParam;
	pSocket->RecvLoop();

	return 0;
}