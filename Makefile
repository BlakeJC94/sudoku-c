P=prg_name
OBJECTS=
CFLAGS = -g -Wall -O3 -DNDEBUG
LDLIBS=
CC=c99

all: sudoku
	./sudoku || true
	./sudoku ./demo.txt

$(P): $(OBJECTS)
