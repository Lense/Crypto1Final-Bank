#include <stdint.h>

typedef struct
{
	uint8_t id;
	uint32_t pin;
	uint64_t balance;
} Account;

typedef struct __attribute__((__packed__))
{
	uint8_t message[6];
	uint64_t session_token;
	uint8_t transaction_num;
} server_to_ATM;

typedef struct __attribute__((__packed__))
{
	uint8_t action;
	uint8_t accounts;
	union
	{
		uint32_t amount;
		uint32_t pin;
	};
	uint64_t session_token;
	uint8_t transaction_num;
} ATM_to_server;
