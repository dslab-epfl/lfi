#include "NetInspector.h"
#include <iostream>
#include <stdarg.h>
#include <string.h>

#define serverHostname  "radu-virtUbuntu9045"
#define serverPort 	11111

NetInspector::NetInspector()
{
    int portno, n;
    struct sockaddr_in serv_addr;
    struct hostent *server;

    portno = serverPort;
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) 
        exit(-1);
    server = gethostbyname(serverHostname);
    if (server == NULL) {
        fprintf(stderr,"ERROR, no such host\n");
        exit(0);
    }
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, 
         (char *)&serv_addr.sin_addr.s_addr,
         server->h_length);
    serv_addr.sin_port = htons(portno);
    if (connect(sockfd,(struct sockaddr*)&serv_addr,sizeof(serv_addr)) < 0) 
        exit(-2);
}

bool NetInspector::Eval(const string& functionName, ...)
{
	/* only intended to be used when intercepting the read function */
	va_list ap;
	int fd;
	size_t size;
	
	int socket_i;
        char *message_i;
        size_t length_i;
        int flags_i;
        struct sockaddr *dest_addr_i;
        socklen_t dest_len_i;

	va_start(ap, functionName);
	socket_i = va_arg(ap, int);
	message_i = va_arg(ap, char*);
	length_i = va_arg(ap, size_t);	
	flags_i = va_arg(ap, int);
	dest_addr_i = va_arg(ap, struct sockaddr *);
	dest_len_i = va_arg(ap, socklen_t);
	va_end(ap);

	bzero(buffer,256);
    	sprintf(buffer, "%s %d",functionName.c_str(), (int)length_i);
    	int n = write(sockfd,buffer,strlen(buffer));
    	if (n < 0) 
     	    exit(-3);
	bzero(receiveBuf,10);
	n = read(sockfd,receiveBuf,10);
	if (n < 0) 
	     exit(4);
	if ( receiveBuf[0] == '1' )
	     return 1;	
	return 0;
}
