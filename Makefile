PWD = $(shell pwd)

CPPFLAGS=-g -O0 
LDFLAGS=-g

OBJS=circuit.o main.o XSynthParser.o

%o: %c
	$(CC) $(CFLAGS) -c $< -o $@

%o: %cpp
	$(CXX) $(CPPFLAGS) -c $< -o $@

all: $(OBJS)
	$(CXX) $(LDFLAGS) -o parser $(OBJS)

.PHONY: clean
clean:
	rm -f *.o parser

.PHONY: dot
dot:
	dot -Tpng graph.dot -o graph.png
