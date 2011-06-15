
#include "../Trigger.h"
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

DEFINE_TRIGGER( NetInspector )
{
public:
	NetInspector();
	bool Eval(const string& functionName, ...);
private:
	int sockfd;	
	char buffer[256];
	char receiveBuf[10];
};
