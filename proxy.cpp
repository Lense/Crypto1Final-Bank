/**
	@file proxy.cpp
	@brief Top level proxy implementation file
 */
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <string.h>

void* client_thread(void* arg);

unsigned short g_bankport;
int main(int argc, char* argv[])
{
	if(argc != 3)
	{
		printf("Usage: proxy listen-port bank-connect-port\n");
		return -1;
	}
	
	unsigned short ourport = atoi(argv[1]);
	g_bankport = atoi(argv[2]);
	
	//socket setup
	int lsock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if(!lsock)
	{
		printf("fail to create socket\n");
		return -1;
	}
	
	//listening address
	sockaddr_in addr_l;
	addr_l.sin_family = AF_INET;
	addr_l.sin_port = htons(ourport);
	unsigned char* ipaddr = reinterpret_cast<unsigned char*>(&addr_l.sin_addr);
	ipaddr[0] = 127;
	ipaddr[1] = 0;
	ipaddr[2] = 0;
	ipaddr[3] = 1;
	if(0 != bind(lsock, reinterpret_cast<sockaddr*>(&addr_l), sizeof(addr_l)))
	{
		printf("failed to bind socket\n");
		return -1;
	}
	if(0 != listen(lsock, SOMAXCONN))
	{
		printf("failed to listen on socket\n");
		return -1;
	}
	
	//loop forever accepting new connections
	while(1)
	{
		sockaddr_in unused;
		socklen_t size = sizeof(unused);
		int csock = accept(lsock, reinterpret_cast<sockaddr*>(&unused), &size);
		if(csock < 0)	//bad client, skip it
			continue;
			
		pthread_t thread;
		pthread_create(&thread, NULL, client_thread, (void*)csock);
	}
}

void* client_thread(void* arg)
{
	int csock = (int)arg;
	
	printf("[proxy] client ID #%d connected\n", csock);
	
	//create a new socket and connect to the bank
	int bsock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if(!bsock)
	{
		printf("fail to create socket\n");
		return NULL;
	}
	sockaddr_in addr_b;
	addr_b.sin_family = AF_INET;
	addr_b.sin_port = htons(g_bankport);
	unsigned char* ipaddr = reinterpret_cast<unsigned char*>(&addr_b.sin_addr);
	ipaddr[0] = 127;
	ipaddr[1] = 0;
	ipaddr[2] = 0;
	ipaddr[3] = 1;
	ipaddr[3] = 1;
	if(0 != connect(bsock, reinterpret_cast<sockaddr*>(&addr_b), sizeof(addr_b)))
	{
		printf("fail to connect to bank\n");
		return NULL;
	}
	
	//input loop
	int length;
	char packet[1024];
	while(1)
	{
		//read the packet from the ATM
		if(sizeof(int) != recv(csock, &length, sizeof(int), 0))
			break;
		if(length >= 1024)
		{
			printf("packet too long\n");
			break;
		}
		if(length != recv(csock, packet, length, 0))
		{
			printf("[proxy] fail to read packet\n");
			break;
		}
		
		//TODO: tamper with packet going from ATM to bank
		
		//forward packet to bank
		if(sizeof(int) != send(bsock, &length, sizeof(int), 0))
		{
			printf("[proxy] fail to send packet length\n");
			break;
		}
		if(length != send(bsock, (void*)packet, length, 0))
		{
			printf("[proxy] fail to send packet\n");
			break;
		}
		
		//get response packet from bank
		if(sizeof(int) != recv(bsock, &length, sizeof(int), 0))
		{
			printf("[proxy] fail to read packet length\n");
			break;
		}
		if(length >= 1024)
		{
			printf("packet too long\n");
			break;
		}
		if(length != recv(bsock, packet, length, 0))
		{
			printf("[proxy] fail to read packet\n");
			break;
		}
		
		//TODO: tamper with packet going from bank to ATM

		//forward packet to ATM
		if(sizeof(int) != send(csock, &length, sizeof(int), 0))
		{
			printf("[proxy] fail to send packet length\n");
			break;
		}
		if(length != send(csock, (void*)packet, length, 0))
		{
			printf("[proxy] fail to send packet\n");
			break;
		}

	}

	printf("[proxy] client ID #%d disconnected\n", csock);
		
	close(bsock);
	close(csock);
	return NULL;
}
