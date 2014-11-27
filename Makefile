all:
	g++ cryptoAPI.cpp -g atm.cpp -Wall -m32 -o atm -lcrypto -lform -lncurses
	g++ cryptoAPI.cpp bank.cpp -Wall -m32 -o bank -lpthread -lcrypto
	g++ proxy.cpp -Wall -m32 -o proxy -lpthread
