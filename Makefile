CC=g++
CFLAGS=-O3
EXTRAFLAGS=-lpqxx -lpq -lpugixml
all: test client

test: createDB.cpp 
	$(CC) $(CFLAGS) -o test createDB.cpp socket.h $(EXTRAFLAGS)

client: client.cpp
	$(CC) $(CFLAGS) -o client client.cpp socket.h $(EXTRAFLAGS)
clean:
	rm -f *~ *.o test client