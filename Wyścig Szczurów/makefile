all: Wyścig\ Szczurów

Wyścig\ Szczurów: server.c tcpConnect.c tcpConnect.h client.h
	gcc -Wall -o wyscigSzczurow server.c tcpConnect.c -lpthread

.PHONY: clean

clean:
	rm signals
