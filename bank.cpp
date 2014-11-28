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
#include "cryptoAPI.h"

// Declare bank accounts
Account bank_accounts[] = {
	{0, 3141592653LL, 100},
	{1, 1619033988LL, 50},
	{2, 2718281828LL, 0}
};

// Declare sessions
uint64_t bank_sessions[3] = { 0 };

void* client_thread(void* arg);
void* console_thread(void* arg);
ATM_to_server receive_and_decrypt(int sock);
void encrypt_and_send(server_to_ATM msg, int sock);
server_to_ATM process_packet(ATM_to_server incoming, server_to_ATM prev_sent);
uint64_t return_balance(const uint8_t id);
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

	pthread_t cthread;
	pthread_create(&cthread, NULL, console_thread, NULL);

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
			current_connect++; // FIXME Have not yet implemented a decrementer for this variable
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

	ATM_to_server incoming;
	server_to_ATM outgoing;


	// TODO:
	// auth/handshake/session stuff should probably happen out here,
	// before we go into the main loop...
	ATM_to_server auth = receive_and_decrypt(csock);
	if(bank_sessions[auth.accounts >> 4] == 0)
	{
		// Assign session token to account
		bank_sessions[auth.accounts >> 4] = incoming.session_token;
	}
	server_to_ATM auth_response = {
		"auth",
		auth.session_token,
		1
	};
	encrypt_and_send(auth_response, csock);
	printf("[bank] user %s authenticated\n", "name");

	while(1)
	{

		// Get, decrypt, parse, and test packet from atm
		incoming = receive_and_decrypt(csock);
		printf("[bank] got packet\n");

		// Make the transaction
		outgoing = process_packet(incoming, outgoing);
		printf("[bank] processed packet\n");

		// Encrypt and send packet back
		encrypt_and_send(outgoing, csock);
		printf("[bank] sent packet\n");
	}

	printf("[bank] client ID #%d disconnected\n", csock);

	close(csock);
	return NULL;
}

ATM_to_server receive_and_decrypt(int sock)
{
	int length = 16;
	unsigned char packet[length+1];

	// Receive packet
	if(length != recv(sock, packet, length, MSG_WAITALL))
	{
		printf("fail to read packet\n");
		pthread_exit(0);
	}

	unsigned char rec_string[length+1];
	// Decrypt packet into message
	if(symmetric_decrypt(packet, rec_string) != length-1)
	{
		printf("failed to decrypt message\n");
		pthread_exit(0);
	}

	ATM_to_server rec;
	memcpy(&rec, rec_string, length-1);
	// TODO check for valid fields
	return rec;
}

void encrypt_and_send(server_to_ATM msg, int sock)
{
	 /*
	  * TODO here:
	  *  test msg
	  */

	int length = 16;
	unsigned char msg_string[length+1];
	memcpy(msg_string, &msg, length-1);
	unsigned char packet[length+1];

	// Encrypt message into packet
	if(symmetric_encrypt(msg_string, packet) != length)
	{
		printf("failed to encrypt message\n");
		pthread_exit(0);
	}

	// Send packet through the proxy to the atm
	if(length != send(sock, (void*)packet, length, 0))
	{
		printf("fail to send packet\n");
		pthread_exit(0);
	}
}

server_to_ATM process_packet(ATM_to_server incoming, server_to_ATM prev_sent)
{
	server_to_ATM outgoing;
	int src = incoming.accounts & 0x0F;
	int dest = incoming.accounts >> 4;

	// TODO check for transaction number rollover
	outgoing.transaction_num = incoming.transaction_num + 1;
	outgoing.session_token = incoming.session_token;

	// TODO check session tokens and transaction numbers
	if(bank_sessions[src] == incoming.session_token && (prev_sent.transaction_num)+2 != outgoing.transaction_num) 
	{
		outgoing.session_token = 0;
		server_to_ATM bad_packet = {"invlg", outgoing.session_token, outgoing.transaction_num};
		return bad_packet;
	}

	// FIXME kill the session or something if the counter rolls over
	if(outgoing.transaction_num == 0)
	{
		outgoing.session_token = 0;
		server_to_ATM bad_packet = {"invlg", outgoing.session_token, outgoing.transaction_num};
		return bad_packet;
	}

	// TODO process things
	switch(incoming.action)
	{
		case 1:
			if(bank_sessions[src] == incoming.session_token) // Check session token
			{
				// FIXME sprintf(outgoing.message, "%d", return_balance(src));
			}
			break;
		case 2:
			if(bank_sessions[src] == incoming.session_token) // Check session token
			{
				if(withdraw_amount(src, incoming.amount))
				{
					//sprintf(outgoing.message, "%d", return_balance(src)); // Return new balance if withdrawal successful
				}
			}	
			break;
		case 3:
			if(bank_sessions[src] == incoming.session_token) // Check session token
			{
				bank_sessions[src] = 0; // Logout
			}
			break;
		case 4:
			if(bank_sessions[src] == incoming.session_token) // Check session token
			{	
				if(transfer_amount(src, incoming.amount, dest))
				{
					// FIXME sprintf(outgoing.message, "%d", return_balance(src)); // Return new balance if transfer successful
				}
			}
			break;
		default:
			pthread_exit(0);
	}
	server_to_ATM to_send = {
		"sampl",
		incoming.session_token,
		(uint8_t)(incoming.transaction_num+1)
	};

	return to_send;
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

uint64_t return_balance(const uint8_t id)
{
	return bank_accounts[id].balance;
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
