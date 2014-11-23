all:
	g++ -lcrypto cryptoAPI.cpp atm.cpp -Wall -m32 -o atm
	g++ -lcrypto cryptoAPI.cpp bank.cpp -Wall -m32 -o bank -lpthread
	g++ proxy.cpp -Wall -m32 -o proxy -lpthread
