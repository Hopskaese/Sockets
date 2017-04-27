#include "Socket.h"

int main()
{
	Socket *pSocket = new Socket();
	pthread_t thread;
	int rc;
	void* status;

	if(!pSocket->Init())
	{
		printf("Error during init\n");
		return -1;
	}

	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

	rc = pthread_create(&thread, &attr, Socket::ThreadStart, pSocket);

	if (rc) 
	{
		printf("Couldnt start thread");
		return -1;
	}

	pSocket->SendPacket(MESSAGE, "s", "Testmessage vom client");
	pSocket->SendPacket(TEST, "sb", "Testmessage TEST", 23213);
	pSocket->SendFile("screenshot2.bmp");

	pthread_attr_destroy(&attr);

	rc = pthread_join(thread, &status);

	if (rc)
	{
        printf("Error:unable to join\n");
        return -1;
    }
	return 0;
}