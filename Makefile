CC=g++
CFLAGS=-O3
EXTRAFLAGS=-lpqxx -lpq
all: test

test: createDB.cpp 
	$(CC) $(CFLAGS) -o test createDB.cpp $(EXTRAFLAGS)

clean:
	rm -f *~ *.o test