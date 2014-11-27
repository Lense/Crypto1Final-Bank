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

// Declare bank accounts
Account bank_accounts[] = {
	{0, 3141592653LL, 100},
	{1, 1619033988LL, 50},
	{2, 2718281828LL, 0}
};

void* client_thread(void* arg);
//void* console_thread(void* arg);
void bank_send(server_to_ATM * msg, int sock);
void bank_recv(ATM_to_server * msg, int sock);
bool deposit_amount(const uint8_t id, const uint64_t amount);
bool withdraw_amount(const uint8_t id, const uint64_t amount);
bool transfer_amount(const uint8_t id, const uint64_t amount, const uint8_t id2);

int main(int argc, char** argv)
{
	if(argc != 2)
	{
		printf("[bank] Usage: bank listen-port\n");
		return -1;
	}

	unsigned short ourport = atoi(argv[1]);

	//socket setup
	int lsock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if(!lsock)
	{
		printf("[bank] fail to create socket\n");
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
		printf("[bank] failed to bind socket\n");
		return -1;
	}
    
	if(0 != listen(lsock, SOMAXCONN))
	{
		printf("[bank] failed to listen on socket\n");
		return -1;
	}

	const int max_connect = 16;
	int current_connect = 0;

	//pthread_t cthread;
	//pthread_create(&cthread, NULL, console_thread, NULL);

	//loop forever accepting new connections
	while(1)
	{
		sockaddr_in unused;
		socklen_t size = sizeof(unused);
		int csock = accept(lsock, reinterpret_cast<sockaddr*>(&unused), &size);
		
        // bad client, skip it
        if(csock < 0)
			continue;

		if(current_connect < max_connect)
		{
			current_connect++; // Have not yet implemented a decrementer for this variable
			pthread_t thread;
			pthread_create(&thread, NULL, client_thread, (void*)csock);
		}
	}
}

void bank_recv(ATM_to_server * msg, int sock)
{

	int length = 16;
	unsigned char packet[16];

	// Receive packet
	if(length != recv(sock, packet, length, 0))
	{
		printf("[bank] fail to read packet\n");
		abort();
	}

	// Decrypt packet into message
	if(symmetric_decrypt(packet, msg) != length)
	{
		printf("[bank] failed to decrypt message\n");
		abort();
	}
    
    // TODO: validate packet
	if(false)
	{
		printf("[bank] packet did not decrypt properly\n");
		abort();
	}

	return;
}

void bank_send(server_to_ATM * msg, int sock)
{

	int length = 16;
	unsigned char packet[16];

	// Encrypt message into packet
	if(symmetric_encrypt(msg, packet) != length)
	{
		printf("[bank] failed to encrypt message\n");
		abort();
	}

	// Send packet through the proxy to the bank
	if(length != send(sock, (void*)packet, length, 0))
	{
		printf("fail to send packet\n");
		abort();
	}

	return;
}

void* client_thread(void* arg)
{
	int csock = (int)arg;

	printf("[bank] client ID #%d connected\n", csock);

	//input loop
    bool result = false;
    
	ATM_to_server incoming;
    server_to_ATM outgoing;
    
    
    // TODO:
    // auth/handshake/session stuff should probably happen out here,
    // before we go into the main loop...
    
	while(1)
	{

        /* read and validate incoming packets */
        bank_recv(&incoming, csock);

		//TODO: process packet data
        processed = process_packet(&incoming, &outgoing);
        
        if(!processed)
            printf("[bank] invalid action occured\n")
        
		// put new data in packet
        bank_send(&outgoing, csock);

	}

	printf("[bank] client ID #%d disconnected\n", csock);

	close(csock);
	return NULL;
}

bool process_packet(ATM_to_server * incoming, server_to_ATM * outgoing)
{

    // For geffory
    
    // Your newly recieved data:
    // incoming->action            == 0-3?
    // incoming->amount            == Used after auth
    // incoming->pin               == Only used during auth
    // (incoming->accounts & 0x0F) == Source Account
    // (incoming->accounts >> 4)   == Destination Account
    // incoming->session_token     == Session token
    // incoming->transaction_num   == Action counter
    // incoming->pin
    
    // Outgoing data:
    // outgoing->message           == I have no idea, ask austin
    // outgoing->session_token     == Session token, should be the same as incoming
    // outgoing->transaction_num   == Get it from incoming, and increment
    
    bool result = false;
    
    outgoing->transaction_num = incoming->transaction_num + 1;
    outgoing->session_token = incoming_session_token;
    
    // kill the session or something if the counter rolls over
    if(outgoing->transaction_num == 0)
    {
        outgoing->session_token = 0;
        return result;
    }
    
    // do your account actions here
    switch(incoming->action)
    {
        case 0:
            break;
        case 1:
            break;
        case 2:
            break;
        case 3:
            break;
            
        default:
            abort();
    }
    
    return result;
}

/*
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
*/

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
