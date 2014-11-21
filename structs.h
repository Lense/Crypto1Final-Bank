#include <stdint.h>

struct Account {
	Account(const uint8_t& my_id, const uint8_t& my_pin, const uint64_t& my_balance) { id = my_id; pin = my_pin; balance = my_balance; }
	//Variables
	uint8_t id;
	uint64_t pin; 
	uint64_t balance;
};

