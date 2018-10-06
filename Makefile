out=asnp
objects = main.o assemble.o

CXX=g++
CPPFLAGS=-std=c++11
LDFLAGS=

$(out): $(objects)
	g++ -o $(out) $(objects) $(CPPFLAGS) $(LDFLAGS)

main.o: assemble.h
assemble.o: assemble.h

.PHONY: clean
clean:
	rm asnp $(objects)
