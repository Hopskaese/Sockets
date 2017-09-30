// WinServer.cpp : Defines the entry point for the console application.
//

#include "Socket.h"

int main()
{
	Socket* pSocket = new Socket();

	std::cout << "Trying to connect to server.." << std::endl;
	if (!pSocket->Init())
		return -1;

	HANDLE hThread = CreateThread(NULL, NULL, (LPTHREAD_START_ROUTINE)Socket::ThreadStart, pSocket, NULL, NULL);
	
	pSocket->RecvLoop();

	TerminateThread(hThread, 0);

	return 0;
}