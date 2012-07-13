objects = main.o

asnp: $(objects)
	g++ -o asnp $(objects)

main.o: assemble.h

.PHONY: clean
clean:
	rm asnp $(objects)
