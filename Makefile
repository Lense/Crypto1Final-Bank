all:
	g++ cryptoAPI.cpp atm.cpp -Wall -m32 -o atm -lcrypto -lform -lncurses
	g++ cryptoAPI.cpp bank.cpp -Wall -m32 -o bank -lpthread -lcrpyto
	g++ proxy.cpp -Wall -m32 -o proxy -lpthread
