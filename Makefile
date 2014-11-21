all:
	g++ atm.cpp -Wall -m32 -o atm
	g++ bank.cpp -Wall -m32 -o bank -lpthread
	g++ proxy.cpp -Wall -m32 -o proxy -lpthread
