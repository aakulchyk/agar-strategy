all:
	g++ main.cpp -std=c++11 -o agar
install:
	cp agar /usr/local/bin/
