CC=g++
CFLAGS=-O3
EXTRAFLAGS=-lpqxx -lpq -lpugixml -lpthread 
all: client

client: client.cpp
	$(CC) $(CFLAGS) -o client client.cpp socket.h $(EXTRAFLAGS)

clean:
	rm -f *~ *.o client