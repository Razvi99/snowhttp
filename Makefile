SRCDIR = lib
BINDIR = bin

CC = g++
FLAGS = -O3 -std=c++17 -pthread 


static: $(SRCDIR)/snowhttp.cpp $(SRCDIR)/snowhttp.h
	$(CC) -c -o $(BINDIR)/snowhttp.o -I$(SRCDIR)/wolf/wolfssl $(FLAGS) $(SRCDIR)/snowhttp.cpp
	$(CC) -c -o $(BINDIR)/events.o $(FLAGS) $(SRCDIR)/events.cpp

	ar rvs $(BINDIR)/snowhttp.a $(BINDIR)/snowhttp.o $(BINDIR)/events.o

	rm $(BINDIR)/*.o

example:
	$(CC) $(FLAGS) example.cpp $(BINDIR)/snowhttp.a $(SRCDIR)/wolf/libwolfssl.a -o $(BINDIR)/example

clean:
	rm $(BINDIR)/*.o

