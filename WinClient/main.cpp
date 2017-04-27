#include "Socket.h"

int main()
{
	Socket* pSocket = new Socket();

	if (!pSocket->Init())
		return -1;

	HANDLE hThread = CreateThread(NULL, NULL, (LPTHREAD_START_ROUTINE)Socket::ThreadStart, pSocket, NULL, NULL);
	pSocket->SendPacket(MESSAGE, "s", "Testmessage vom client");
	pSocket->SendPacket(TEST, "sb", "Testmessage TEST", 23213);
	pSocket->SendFile("screenshot.bmp");

	if (hThread)
		return WaitForSingleObject(hThread, INFINITE);
	else
		return -1;

	return 0;
}