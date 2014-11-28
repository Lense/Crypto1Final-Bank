/**
	@file atm.cpp
	@brief Top level ATM implementation file
 */
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <form.h>
#include <openssl/rand.h>
#include <inttypes.h>
#include "structs.h"
#include "cryptoAPI.h"

server_to_ATM encrypt_and_send(ATM_to_server msg, int sock)
{
	int length = 16;
	unsigned char msg_string[length+1];
	memcpy(msg_string, &msg, length-1);
	unsigned char packet[length+1];

	// Encrypt message into packet
	if(symmetric_encrypt(msg_string, packet) != length)
	{
		printf("failed to encrypt message\n");
		abort();
	}

	// Send packet through the proxy to the bank
	if(send(sock, (void*)packet, length, 0) != length)
	{
		printf("fail to send packet\n");
		abort();
	}

	//printf("sent packet %u %u %u %" PRIu64 " %u\n", msg.action, msg.accounts, msg.amount, msg.session_token, msg.transaction_num); //FIXME remove this

	// Receive packet
	if(length != recv(sock, packet, length, MSG_WAITALL))
	{
		printf("fail to read packet\n");
		abort();
	}

	unsigned char rec_string[length+1];
	// Decrypt packet into message
	if(symmetric_decrypt(packet, rec_string) != length-1)
	{
		printf("failed to decrypt message\n");
		abort();
	}

	server_to_ATM rec;
	memcpy(&rec, rec_string, length-1);
	//printf("received packet %s %" PRIu64 " %u\n", rec.message, rec.session_token, rec.transaction_num); //FIXME remove this

	// Test session token and session transaction number
	if(msg.session_token != rec.session_token)
	{
		printf("\nPossible spoofing detected with session token\n");
		abort();
	}
	if(rec.transaction_num != 0xFF || strncmp(rec.message, "disc", 4) != 0)
	{
		if(msg.transaction_num+1 != rec.transaction_num)
		{
			printf("\nPossible spoofing detected with transaction number\n");
			abort();
		}
	}
	// Bank might have disconnected here but that's ok because we check for that in caller
	return rec;
}

void input_loop(int sock, ATM_to_server auth)
{
	// Begin boilerplate
	FIELD* action_field[2];
	FIELD* withdraw_field[2];
	FIELD* transfer_field[3];
	FORM* action_form;
	FORM* withdraw_form;
	FORM* transfer_form;
	FORM* cur_form;
	int ch;

	initscr();
	cbreak();
	noecho();
	keypad(stdscr, true);
	// End boilerplate


	// FIXME Valgrind says something weird is going on with these,
	//   but the code seems to work
	const char* valid_actions[5] = {"balance", "withdraw", "logout", "transfer", NULL};
	const char* valid_account_names[4] = {"Alice", "Bob", "Eve", NULL};

	int logged_in = 1;
	while(logged_in)
	{

		// Action prompt
		action_field[0] = new_field(1, 10, 4, 18, 0, 0);
		action_field[1] = NULL;
		set_field_type(action_field[0], TYPE_ENUM, valid_actions, 0, 0);
		set_field_back(action_field[0], A_UNDERLINE);
		field_opts_off(action_field[0], O_AUTOSKIP);

		action_form = new_form(action_field);
		cur_form = action_form;
		post_form(action_form);

		// Prompt
		mvprintw(4, 10, "action:");
		refresh();

		// Character input loop
		uint8_t action;
		unsigned char loop = 1;
		unsigned int fields_entered = 0;
		while(loop)
		{
			ch = getch();
			switch(ch)
			{
				form_driver(cur_form, REQ_VALIDATION);
				case KEY_DOWN:
				case 0x9:
				case ' ':
					form_driver(cur_form, REQ_NEXT_FIELD);
					form_driver(cur_form, REQ_END_LINE);
					break;
				case KEY_UP:
					form_driver(cur_form, REQ_PREV_FIELD);
					form_driver(cur_form, REQ_END_LINE);
					break;
				case KEY_BACKSPACE:
				case KEY_DC:
				case 127:
					form_driver(cur_form, REQ_CLR_FIELD);
					break;
				case 0xa:
					switch(form_driver(cur_form, REQ_VALIDATION))
					{
						case E_OK:
							switch(fields_entered)
							{
								case 0:
									if(field_buffer(action_field[0],0)[0]!=' ')
									{
										fields_entered = 1;
									}
									else
									{
										mvprintw(8, 10, "form not completely filled");
										break;
									}
								case 1:
									switch(field_buffer(action_field[0], 0)[0])
									{
										case 'b':
											action = 1;
											loop = 0;
											break;
										case 'w':
											action = 2;

											// amount prompt
											withdraw_field[0] = new_field(1, 10, 8, 18, 0, 0);
											withdraw_field[1] = NULL;
											set_field_back(withdraw_field[0], A_UNDERLINE);
											field_opts_off(withdraw_field[0], O_AUTOSKIP);
											set_field_type(withdraw_field[0], TYPE_INTEGER, 0, 1, 0xfffffffe);

											withdraw_form = new_form(withdraw_field);
											cur_form = withdraw_form;
											post_form(withdraw_form);
											refresh();

											fields_entered++; // close enough
											mvprintw(8, 10, "amount:");
											refresh();
											break;
										case 'l':
											action = 3;
											logged_in = 0;
											loop = 0;
											break;
										case 't':
											action = 4;

											// Dest account prompt
											transfer_field[0] = new_field(1, 10, 6, 18, 0, 0);
											set_field_back(transfer_field[0], A_UNDERLINE);
											field_opts_off(transfer_field[0], O_AUTOSKIP);
											set_field_type(transfer_field[0], TYPE_ENUM, valid_account_names, 0, 0);

											// amount account prompt
											transfer_field[1] = new_field(1, 10, 8, 18, 0, 0);
											set_field_back(transfer_field[1], A_UNDERLINE);
											field_opts_off(transfer_field[1], O_AUTOSKIP);
											set_field_type(transfer_field[1], TYPE_INTEGER, 0, 1, 0xfffffffe);

											transfer_field[2] = NULL;

											transfer_form = new_form(transfer_field);
											cur_form = transfer_form;
											post_form(transfer_form);
											mvprintw(6, 10, "account:");
											mvprintw(8, 10, "amount:");
											refresh();

											fields_entered++; // close enough
											break;
										default:
											abort();
									}
									break;
								case 2:
									switch(action)
									{
										case 2: // withdraw
											if(field_buffer(withdraw_field[0],0)[0]!=' ' && \
												field_buffer(withdraw_field[0],0)[0]!='-')
												loop = 0;
											break;
										case 4: // transfer
											if(field_buffer(transfer_field[0],0)[0]!=' ' && \
													field_buffer(transfer_field[1],0)[0]!=' ' && \
													field_buffer(transfer_field[1],0)[0]!='-')
												loop = 0;
											break;
										default:
											abort();
									}
									break;
								default:
									abort();
							}
							break;
						case E_BAD_ARGUMENT:
							mvprintw(9, 10, "bad argument");
							break;
						case E_BAD_STATE:
							mvprintw(9, 10, "bad state");
							break;
						case E_NOT_POSTED:
							mvprintw(9, 10, "not posted");
							break;
						case E_INVALID_FIELD:
							mvprintw(9, 10, "invalid field");
							break;
						case E_REQUEST_DENIED:
							mvprintw(9, 10, "request denied");
							break;
						case E_SYSTEM_ERROR:
							mvprintw(9, 10, "system error");
							break;
						default:
							mvprintw(9, 10, "something else");
							abort();
					}
				default:
					form_driver(cur_form, ch);
					break;
			}
		}
		uint8_t account_dst = 0;
		uint8_t amount = 0;
		if(action == 4) // transfer
		{
			switch(field_buffer(transfer_field[0], 0)[0])
			{
				case 'A':
					account_dst = 0;
					break;
				case 'B':
					account_dst = 1;
					break;
				case 'E':
					account_dst = 2;
					break;
				default:
					abort();
			}
			amount = (uint8_t)atoi(field_buffer(transfer_field[1], 0));
			free_field(transfer_field[0]);
			free_field(transfer_field[1]);
			unpost_form(transfer_form);
			free_form(transfer_form);
		}
		else if(action == 2) // withdraw
		{
			amount = (uint32_t)atoi(field_buffer(withdraw_field[0], 0));
			free_field(withdraw_field[0]);
			unpost_form(withdraw_form);
			free_form(withdraw_form);
		}
		ATM_to_server msg = {
			action,
			(uint8_t)((auth.accounts & 0xf0) | (account_dst & 0x0f)),
			amount,
			0, // Session token
			0 // Session transaction number
		};
		server_to_ATM rec = encrypt_and_send(msg, sock);

		if(rec.transaction_num == 0xFF && strncmp(rec.message, "disc", 4) == 0)
		{
			mvprintw(14, 10, "The bank has terminated your session");
			logged_in = 0;
		}
		else if(action==1 || action == 2 || action == 4) // balance
		{
			uint32_t balance;
			memcpy(&balance, rec.message, 4);
			mvprintw(15, 10, "Your balance is \"%u\"", balance);
		}
		else
		{
			mvprintw(14, 10, "Received \"%s\" from bank", rec.message);
		}
		refresh();

		sleep(2);

		unpost_form(action_form);
		free_form(action_form);
		free_field(action_field[0]);
	}
	endwin();
}

ATM_to_server authenticate_credentials()
{
	FIELD *field[3];
	FORM  *my_form;
	int ch;

	initscr();
	start_color();
	cbreak();
	noecho();
	keypad(stdscr, true);
	init_color(COLOR_GREEN, 100, 100, 100);
	init_pair(1, COLOR_GREEN, COLOR_BLACK);
	attron(COLOR_PAIR(1));

	field[0] = new_field(1, 5, 4, 24, 0, 0);
	field[1] = new_field(1, 10, 6, 24, 0, 0);
	field[2] = NULL;

	// Field options
	set_field_back(field[0], A_UNDERLINE);
	field_opts_off(field[0], O_AUTOSKIP);
	field_opts_off(field[0], O_NULLOK);
	set_field_back(field[1], A_UNDERLINE);
	field_opts_off(field[1], O_AUTOSKIP);
	field_opts_on(field[1], O_STATIC);
	if(field_opts_on(field[1], O_NULLOK) != E_OK)
		abort();
	const char* valid_account_names[4] = {"Alice", "Bob", "Eve", NULL};
	set_field_type(field[0], TYPE_ENUM, valid_account_names, 0, 0);
	set_field_type(field[1], TYPE_NUMERIC, 0, 1, 0xfffffffe);

	my_form = new_form(field);
	post_form(my_form);
	refresh();

	// Prompts
	mvprintw(4, 10, "Account name:");
	mvprintw(6, 10, "         PIN:");
	refresh();

	// Character input loop
	unsigned char loop = 1;
	while(loop)
	{
		ch = getch();
		switch(ch)
		{
			form_driver(my_form, REQ_VALIDATION);
			case KEY_DOWN:
			case 0x9:
			case ' ':
				form_driver(my_form, REQ_NEXT_FIELD);
				form_driver(my_form, REQ_END_LINE);
				break;
			case KEY_UP:
				form_driver(my_form, REQ_PREV_FIELD);
				form_driver(my_form, REQ_END_LINE);
				break;
			case KEY_BACKSPACE:
			case KEY_DC:
			case 127:
				form_driver(my_form, REQ_CLR_FIELD);
				break;
			case 0xa:
				switch(form_driver(my_form, REQ_VALIDATION))
				{
					case E_OK:
						if(field_buffer(field[0],0)[0]!=' ' && \
								field_buffer(field[1],0)[0]!=' ')
							loop = 0;
						else
							mvprintw(8, 10, "form not completely filled");
						break;
					case E_BAD_ARGUMENT:
						mvprintw(8, 10, "bad argument");
						break;
					case E_BAD_STATE:
						mvprintw(8, 10, "bad state");
						break;
					case E_NOT_POSTED:
						mvprintw(8, 10, "not posted");
						break;
					case E_INVALID_FIELD:
						mvprintw(8, 10, "invalid field");
						break;
					case E_REQUEST_DENIED:
						mvprintw(8, 10, "invalid field");
						break;
					case E_SYSTEM_ERROR:
						mvprintw(8, 10, "system error");
						break;
					default:
						mvprintw(8, 10, "something else");
						break;
				}
			default:
				form_driver(my_form, ch);
				break;
		}
	}

	// Read account number from card
	FILE* card_file;
	switch(field_buffer(field[0], 0)[0])
	{
		case 'A':
			card_file = fopen("Alice.card", "r");
			break;
		case 'B':
			card_file = fopen("Bob.card", "r");
			break;
		case 'E':
			card_file = fopen("Eve.card", "r");
			break;
		default:
			abort();
	}
	if(!card_file)
	{
		printf("Bad atm card\n");
		abort();
	}
	uint8_t account_number;
	if(fscanf(card_file, "%" SCNu8, &account_number) != 1)
		abort();
	if(account_number > 2)
	{
		printf("Bad atm card\n");
		abort();
	}
	fclose(card_file);

	// Get random session token
	unsigned char rand_str[9];
	if(RAND_bytes(rand_str, 8) != 1)
	{
		printf("failed to get high-quality randomness\n");
		abort();
	}
	
	uint64_t session_token;
	if(!memcpy(&session_token, rand_str, 8))
		abort();

	// atoi does not parse past INT_MAX, must use sscanf
	uint32_t pin = 0;
	sscanf(field_buffer(field[1], 0), "%u", &pin);

	// auth struct to be returned
	ATM_to_server auth = {
		0, // Action login
		(uint8_t)(account_number << 4),
		pin, // PIN
		session_token,
		0
	};

	// Cleanup
	unpost_form(my_form);
	free_form(my_form);
	free_field(field[0]);
	free_field(field[1]);
	endwin();

	return auth;
}

int main(int argc, char* argv[])
{
	if(argc != 2)
	{
		printf("Usage: atm proxy-port\n");
		return -1;
	}

	//socket setup
	unsigned short proxport = atoi(argv[1]);
	int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if(!sock)
	{
		printf("fail to create socket\n");
		return -1;
	}
	sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(proxport);
	unsigned char* ipaddr = reinterpret_cast<unsigned char*>(&addr.sin_addr);
	ipaddr[0] = 127;
	ipaddr[1] = 0;
	ipaddr[2] = 0;
	ipaddr[3] = 1;
	if(0 != connect(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)))
	{
		printf("fail to connect to proxy\n");
		return -1;
	}

	// Here is where our code starts
	ATM_to_server auth = authenticate_credentials();
	server_to_ATM rec = encrypt_and_send(auth, sock);
	if(rec.transaction_num == 0xFF && strncmp(rec.message, "disc", 4) == 0)
	{
		printf("Failed to authenticate\n");
		abort();
	}
	fflush(0);
	input_loop(sock, auth);

	//cleanup
	close(sock);
	return 0;
}
