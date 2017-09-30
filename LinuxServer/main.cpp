#include "Socket.h"
#include <signal.h>

Socket* pSocket = NULL;

void InterruptHandler(int signum)
{
	printf("Caught interrupt. closing socket\n");
	if (pSocket != NULL)
		delete pSocket;
	exit(signum);
}

int main()
{
	pSocket = new Socket();
	pthread_t thread;
	int rc;
	void* status;
	signal(SIGINT, InterruptHandler);

	if (!pSocket->Init())
	{
		printf("Error during init\n");
		return -1;
	}
	pSocket->AcceptLoop();

	return 0;
}