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
#include "structs.h"

server_to_ATM encrypt_and_send(ATM_to_server msg)
{
	 /*
	  * TODO here:
	  *  test msg (maybe?)
	  *  encrypt msg
	  *  put it in buf
	  *  send (sample code below)
	  *  check return
	  *  decrypt
	  *  check valid return
	  *  parse packet into struct
	  */
	

	/*
	char packet[16];
	int length = 1;
	//send the packet through the proxy to the bank
	if(sizeof(int) != send(sock, &length, sizeof(int), 0))
	{
		printf("fail to send packet length\n");
		abort();
	}
	if(length != send(sock, (void*)packet, length, 0))
	{
		printf("fail to send packet\n");
		abort();
	}
	
	//TODO: do something with response packet
	if(sizeof(int) != recv(sock, &length, sizeof(int), 0))
	{
		printf("fail to read packet length\n");
		abort();
	}
	if(length >= 1024)
	{
		printf("packet too long\n");
		abort();
	}
	if(length != recv(sock, packet, length, 0))
	{
		printf("fail to read packet\n");
		abort();
	}
	*/

	// SAMPLE to make it compile
	server_to_ATM rec = {
		"heyth",
		0,
		0
	};
	return rec;
}

void input_loop(int sock, ATM_to_server auth, server_to_ATM initial_rec)
{
	/*
	 * TODO here:
	 *  add options for different actions
	 *  make it actually loop
	 *  show what was received by server
	 */



	FIELD *field[3];
	FORM  *my_form;
	int ch;
	


	initscr();
	cbreak();
	noecho();
	keypad(stdscr, true);
	
	//FIELD *new_field(int height, int width, int toprow, int leftcol, int offscreen, int nbuffers);
	field[0] = new_field(1, 10, 4, 18, 0, 0);
	field[1] = new_field(1, 10, 6, 18, 0, 0);
	field[2] = new_field(1, 10, 8, 18, 0, 0);
	field[3] = NULL;

	// Field options
	set_field_back(field[0], A_UNDERLINE);
	field_opts_off(field[0], O_AUTOSKIP);
	set_field_back(field[1], A_UNDERLINE);
	field_opts_off(field[1], O_AUTOSKIP);
	set_field_back(field[2], A_UNDERLINE);
	field_opts_off(field[2], O_AUTOSKIP);
	// Testing
	set_field_type(field[0], TYPE_INTEGER, 10, 0, 0xffffffff);

	my_form = new_form(field);
	post_form(my_form);
	refresh();
	
	// Prompts
	mvprintw(4, 10, "action:");
	mvprintw(6, 10, "account:");
	mvprintw(8, 10, "amount:");
	refresh();

	// Character input loop
	while((ch = getch()) != 0xa)
	{
		switch(ch)
		{
				case KEY_DOWN:
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
			default:
				form_driver(my_form, ch);
				break;
		}
		form_driver(my_form, REQ_VALIDATION);
	}
	ATM_to_server msg = {
		(uint8_t)atoi(field_buffer(field[0], 0)),
		(uint8_t)atoi(field_buffer(field[1], 0)),
		(uint8_t)atoi(field_buffer(field[2], 0)),
		0,
		0
	};
	server_to_ATM rec = encrypt_and_send(msg);

	// It should loop around here
	
	unpost_form(my_form);
	free_form(my_form);
	free_field(field[0]);
	free_field(field[1]);
	free_field(field[2]);
	endwin();
}

ATM_to_server authenticate_credentials()
{
	/*
	 * TODO here:
	 *  add better prompts with more usage info
	 *  set field options (correctly) so it only takes numbers
	 *  figure out why freeing segfaults here but not in standalone code
	 *  make prettier
	 */



	FIELD *field[2];
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

	//FIELD *new_field(int height, int width, int toprow, int leftcol, int offscreen, int nbuffers);
	field[0] = new_field(1, 10, 4, 24, 0, 0);
	field[1] = new_field(1, 10, 6, 24, 0, 0);
	field[2] = NULL;

	// Field options
	set_field_back(field[0], A_UNDERLINE);
	field_opts_off(field[0], O_AUTOSKIP);
	set_field_back(field[1], A_UNDERLINE);
	field_opts_off(field[1], O_AUTOSKIP);

	my_form = new_form(field);
	post_form(my_form);
	refresh();
	
	// Prompts
	mvprintw(4, 10, "Account name:");
	mvprintw(6, 10, "         PIN:");
	refresh();

	// Character input loop
	while((ch = getch()) != 0xa)
	{
		switch(ch)
		{
			case KEY_DOWN:
				form_driver(my_form, REQ_NEXT_FIELD);
				form_driver(my_form, REQ_END_LINE);
				break;
			case KEY_UP:
				form_driver(my_form, REQ_PREV_FIELD);
				form_driver(my_form, REQ_END_LINE);
				break;
			default:
				form_driver(my_form, ch);
				break;
		}
		form_driver(my_form, REQ_VALIDATION);
	}

	ATM_to_server auth = {
		0,
		(uint8_t)atoi(field_buffer(field[0], 0)),
		(uint8_t)atoi(field_buffer(field[1], 0)),
		0, // session token 0?
		0
	};
	
	//unpost_form(my_form); // weird segfaults here TODO
	free_form(my_form);
	free_field(field[0]);
	free_field(field[1]);
	endwin();

	return auth;
}

int main(int argc, char* argv[])
{
	/*
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
	*/
	
	int sock=-1; // To help it compile without needing everything running
	// Here is where our code starts
	ATM_to_server auth = authenticate_credentials();
	server_to_ATM rec = encrypt_and_send(auth);
	input_loop(sock, auth, rec);
	
	//cleanup
	close(sock);
	return 0;
}
