#include <thread>
#include "CommThread.h"
#include "NamedPipeServer.h"

static std::thread gCommThread;
static bool gQuit = false;

static void Communication(Service* srv)
{
	NamedPipeServer nps(srv);

	nps.RunServer();
}

void StartCommunicationThread(Service* srv)
{
	gCommThread = std::thread(Communication, srv);
}

void StopCommunicationThread()
{
	if (gCommThread.joinable()) {
		gQuit = true;
		gCommThread.join();
	}
}