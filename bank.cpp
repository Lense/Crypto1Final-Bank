/**
	@file bank.cpp
	@brief Top level bank implementation file
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
#include "structs.h"

// Declare bank accounts, w/ associated mutexes (FIXME no mutexes?), as global variable
Account bank_accounts[] = {
	{0, 3141592653LL, 100},
	{1, 1619033988LL, 50},
	{2, 2718281828LL, 0}
};

void* client_thread(void* arg);
void* console_thread(void* arg);
bool deposit_amount(const uint8_t id, const uint64_t amount);
bool withdraw_amount(const uint8_t id, const uint64_t amount);
bool transfer_amount(const uint8_t id, const uint64_t amount, const uint8_t id2);

int main(int argc, char** argv)
{
	if(argc != 2)
	{
		printf("Usage: bank listen-port\n");
		return -1;
	}

	unsigned short ourport = atoi(argv[1]);

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

	const int max_connect = 16;
	int current_connect = 0;

	pthread_t cthread;
	pthread_create(&cthread, NULL, console_thread, NULL);

	//loop forever accepting new connections
	while(1)
	{
		sockaddr_in unused;
		socklen_t size = sizeof(unused);
		int csock = accept(lsock, reinterpret_cast<sockaddr*>(&unused), &size);
		if(csock < 0)	//bad client, skip it
			continue;

		if(current_connect < max_connect)
		{
			current_connect++; // Have not yet implemented a decrementer for this variable
			pthread_t thread;
			pthread_create(&thread, NULL, client_thread, (void*)csock);
		}
	}
}

void* client_thread(void* arg)
{
	int csock = (int)arg;

	printf("[bank] client ID #%d connected\n", csock);

	//input loop
	int length;
	char packet[1024];
	while(1)
	{
		//read the packet from the ATM
		if(sizeof(int) != recv(csock, &length, sizeof(int), 0))
			break;
		if(length >= 1024 || length <= 0) // Updated if statement requirements
		{
			printf("invalid packet length\n");
			break;
		}
		if(length != recv(csock, packet, length, 0))
		{
			printf("[bank] fail to read packet\n");
			break;
		}

		//TODO: process packet data

		//TODO: put new data in packet

		//send the new packet back to the client
		if(sizeof(int) != send(csock, &length, sizeof(int), 0))
		{
			printf("[bank] fail to send packet length\n");
			break;
		}
		if(length != send(csock, (void*)packet, length, 0))
		{
			printf("[bank] fail to send packet\n");
			break;
		}

	}

	printf("[bank] client ID #%d disconnected\n", csock);

	close(csock);
	return NULL;
}

void* console_thread(void* arg)
{
	char buf[80];
	while(1)
	{
		printf("bank> ");
		fgets(buf, 79, stdin);
		buf[strlen(buf)-1] = '\0';	//trim off trailing newline

		//TODO: your input parsing code has to go here
	}
}

bool deposit_amount(const uint8_t id, const uint64_t amount)
{
	if(amount < 0 || id < 0 || id > 2) return false;
	uint64_t temp_balance = bank_accounts[id].balance;
	temp_balance += amount;
	if(temp_balance < bank_accounts[id].balance) return false;
	bank_accounts[id].balance = temp_balance;
	return true;
}

bool withdraw_amount(const uint8_t id, const uint64_t amount)
{
	if(amount < 0 || id < 0 || id > 2) return false;
	if(bank_accounts[id].balance < amount) return false;
	uint64_t temp_balance = bank_accounts[id].balance;
	temp_balance -= amount;
	if(temp_balance > bank_accounts[id].balance) return false;
	bank_accounts[id].balance = temp_balance;
	return true;
}

bool transfer_amount(const uint8_t id, const uint64_t amount, const uint8_t id2)
{
	if(amount < 0 || id < 0 || id > 2) return false;
	if(id == id2 || id2 < 0 || id2 > 2) return false;
	if(bank_accounts[id].balance < amount) return false;
	uint64_t temp_balance1 = bank_accounts[id].balance;
	//uint64_t temp_balance2 = bank_accounts[id2].balance; // FIXME unused
	if(!withdraw_amount(id, amount)) return false;
	if(!deposit_amount(id2, amount))
	{
		bank_accounts[id].balance = temp_balance1; //Reset account 1 if this action fails.
		return false;
	}
	return true;
}
