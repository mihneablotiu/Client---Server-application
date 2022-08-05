CPPC = g++
CFLAGS = -Wall -Wextra

build: server subscriber

run-server:
	./server

run-subscriber:
	./subscriber

server: server.cpp
	$(CPPC) -g server.cpp -o server $(CFLAGS)
	
subscriber: subscriber.cpp
	$(CPPC) -g subscriber.cpp -o subscriber $(CFLAGS)

.PHONY: clean


pack:
	zip -FSr 323CA_BlotiuMihnea-Andrei_Tema2.zip readme.txt Makefile *.cpp *.h

clean:
	rm -rf *.o server subscriber
