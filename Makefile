CC = g++
CPPFLAGS = -std=c++0x -Wall -ansi -pedantic -g
DISBALEDOPTIMIZATIONFLAGS = -O2 -DNDEBUG
OBJ = InputReader.o Tree.o TreeController.o TreeControllerSimple.o

all: main main-simple rand-matrix test

main: main.o $(OBJ)
	$(CC) $(CPPFLAGS) -o main main.o $(OBJ)

main-simple: main-simple.o $(OBJ)
	$(CC) $(CPPFLAGS) -o main-simple main-simple.o $(OBJ)

rand-matrix: rand-matrix.o
	$(CC) $(CPPFLAGS) -o rand-matrix rand-matrix.o

test: main
	./main --input test.input --plaintext --verbose --debug 1000 --dot test --rewind
	./main --input test.input2 --plaintext --verbose --debug 1000 --dot test --rewind
	./main --input test.input3 --plaintext --verbose --debug 1000 --dot test --rewind
	./main --input test.input4 --plaintext --verbose --debug 1000 --dot test --rewind

dot: main
	for i in *.dot; do dot -Tpng $$i > $${i%.dot}.png; done

clean:
	rm -f main *.o *~ *.dot *.png

depend:
	g++ -MM -std=c++0x *.cpp > dependencies.mk

include dependencies.mk
