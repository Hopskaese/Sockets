#include "Socket.h"

Socket::Socket() 
{
	m_pConnectSocket = NULL;
	m_ServerState = false;
}

bool Socket::Init()
{
	WSADATA wsaData;
	struct addrinfo *result = NULL, hints;
	int iResult;

	if ((iResult = WSAStartup(MAKEWORD(2, 2), &wsaData)) != 0)
	{
		printf("WSAStartup failed with error: %d\n", iResult);
		return false;
	}

	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	if (getaddrinfo(SERVER, PORT, &hints, &result) != 0)
	{
		PrintErrorMsg();
		WSACleanup();
		return false;
	}

	if ((m_pConnectSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol)) == INVALID_SOCKET)
	{
		PrintErrorMsg();
		WSACleanup();
		return false;
	}

	if ((iResult = connect(m_pConnectSocket, result->ai_addr, (int)result->ai_addrlen)) == SOCKET_ERROR)
	{
		PrintErrorMsg();
		closesocket(m_pConnectSocket);
		return false;
	}
	return true;
}

void Socket::Process(Packet packet)
{
	switch (packet.byType)
	{
	case S2C_MESSAGE:
	{
		char* pMessage;
		ReadPacket(packet.data, "s", &pMessage);
		printf("Nachricht: %s\n", pMessage);
	}
	break;
	case S2C_DOCUMENT:
	{
		int bSize = 0;
		char* pName = NULL;
		ReadPacket(packet.data, "bs", &bSize, &pName);
		ReadInFile(bSize, pName);
	}
	break;
	case S2C_STATE:
	{
		ReadPacket(packet.data, "b", &m_ServerState);
		std::cout << "Server is ready to receive. Notifying" << std::endl;
		m_Cv.notify_all();
	}
	break;
	}
}

void Socket::InteractionLoop()
{
	std::string buffer;
	while (true)
	{
		std::cout << "Options: send file / send message / quit to exit" << std::endl;
		getline(std::cin, buffer);
		if (buffer == "send file")
		{
			OPENFILENAME ofn;
			wchar_t File[260];
			HWND hwnd = NULL;
			HANDLE hf = NULL;

			ZeroMemory(&ofn, sizeof(ofn));
			ofn.lStructSize = sizeof(ofn);
			ofn.hwndOwner = hwnd;
			ofn.lpstrFile = File;
			ofn.lpstrFile[0] = '\0';
			ofn.nMaxFile = sizeof(File);
			ofn.lpstrFilter = L"All\0*.*\0Text\0*.TXT\0";
			ofn.nFilterIndex = 1;
			ofn.lpstrFileTitle = NULL;
			ofn.nMaxFileTitle = 0;
			ofn.lpstrInitialDir = NULL;
			ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

			if (GetOpenFileName(&ofn) == true)	
				SendFile(ws2s(std::wstring(ofn.lpstrFile)));
		}
		else if (buffer == "send message")
		{
			std::string msg;
			std::cout << "Put in your message" << std::endl;
			getline(std::cin, msg);
			SendPacket(C2S_MESSAGE, "s",  msg.c_str());
		}
		else if (buffer == "quit")
		{
			closesocket(m_pConnectSocket);
			return;
		}
		else
		{
			std::cout << "Wrong input" << std::endl;
		}
		printf("\n");
	}
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

	if (send(m_pConnectSocket, (char*)&packet, packet.bSize, 0) == SOCKET_ERROR)
	{
		closesocket(m_pConnectSocket);
		WSACleanup();

		return false;
	}
	return true;
}

void Socket::RecvLoop()
{
	int bLen = 0;
	PACKETBUFFER pBuffer;
	while (true)
	{
		if ((bLen = recv(m_pConnectSocket, pBuffer, sizeof(PACKETBUFFER), 0)) == SOCKET_ERROR)
		{
			PrintErrorMsg();
			closesocket(m_pConnectSocket);
			WSACleanup();
			break;
		}

		char* p = pBuffer;
		while (bLen > 0 && bLen >= *(int*)p)
		{
			Packet packet;
			memset(&packet, 0, sizeof(Packet));
			//copying packet into new Packet
			memcpy(&packet, p, *(int*)p);

			if (packet.byType == S2C_DISCONNECT)
			{
				std::cerr << "The server went offline. Closing" << std::endl;
				return;
			}
			Process(packet);

			// length left to read is lessen by the last packet size
			bLen -= *(int*)p;
			// pointer increased to next packet
			p += *(int*)p;
		}
	}
}

void Socket::SendFile(std::string name)
{
	FILE *pFile;
	errno_t err;
	int bOffset = 0;
	int bSentCount = 0;
	int bReadCount = 0;
	int bTotal = 0;
	char Buffer[BUFFERSIZE];

	if ((err = fopen_s(&pFile, name.c_str(), "rb")) != NULL)
	{
		std::cerr << "Unable to open file: " << name << " Error: " << strerror(err) << std::endl;
		return;
	}
	
	fseek(pFile, 0, SEEK_END);
	int bLen = ftell(pFile);
	rewind(pFile);

	if (!SendPacket(C2S_DOCUMENT, "bs", bLen, GetFileName(name).c_str()))
	{
		fclose(pFile);
		std::cerr << "Unable to send Document Information. Not going to send file" << std::endl;
		std::cerr << "Error Message: "; PrintErrorMsg();
		return;
	}
	std::unique_lock<std::mutex>lk(m_Mtx);
	m_Cv.wait(lk);
	if (m_ServerState)
	{
		do
		{
			bOffset = 0;
			bReadCount = fread(Buffer, sizeof(char), BUFFERSIZE, pFile);
			while (bOffset < bReadCount)
			{
				if ((bSentCount = send(m_pConnectSocket, Buffer, bReadCount - bOffset, 0)) == SOCKET_ERROR)
				{
					PrintErrorMsg();
					closesocket(m_pConnectSocket);
					WSACleanup();
					fclose(pFile);
					return;
				}
				bOffset += bSentCount;
				bTotal += bSentCount;
			}
		} while (bReadCount == BUFFERSIZE);
		printf("Bytes sent: %d\n", bTotal);
	}
	else
	{
		std::cout << "A serverside error occured. Will not send data" << std::endl;
	}
	fclose(pFile);
}

void Socket::ReadInFile(int size, char* name)
{
	printf("About to receive file of Size: %d Bytes\n", size);
	int bRecvCount = 0;
	int bReceived = 0;
	char Buffer[BUFFERSIZE];
	FILE* pFile;
	errno_t err;

	if ((err = fopen_s(&pFile, name, "rb")) != 0)
	{
		fprintf_s(stderr, "cannot open file '%s': %s\n", name, strerror(err));
		SendPacket(C2S_STATE, "b", false);
		return;
	}
	SendPacket(C2S_STATE, "b", true);
	do
	{
		if ((bRecvCount = recv(m_pConnectSocket, Buffer, BUFFERSIZE, 0)) == SOCKET_ERROR)
		{
			PrintErrorMsg();
			closesocket(m_pConnectSocket);
			WSACleanup();
			fclose(pFile);
			return;
		}
		fwrite(Buffer, 1, bRecvCount, pFile);
		bReceived += bRecvCount;
	} while (bReceived < size);
	std::cout << "File has been successfully received" << std::endl;
	fclose(pFile);
}

DWORD Socket::ThreadStart(LPVOID lParam)
{
	Socket *pSocket = (Socket*)lParam;
	pSocket->InteractionLoop();

	return 0;
}

void Socket::PrintErrorMsg()
{
	LPWSTR errString = NULL;
	FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		WSAGetLastError(),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), 
		(LPWSTR)&errString, 0, NULL);

	std::wcout << errString << std::endl;
	HeapFree(GetProcessHeap(), 0, errString);
}