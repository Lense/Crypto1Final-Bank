all:
	g++ atm.cpp -m32 -o atm
	g++ bank.cpp -m32 -o bank -lpthread
	g++ proxy.cpp -m32 -o proxy -lpthread
