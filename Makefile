.PHONY: all clean disassemble

all: test

test: region.cpp $(wildcard *.h)
	g++ -std=c++14 -O3 -Wall -o $@ $<

clean:
	$(RM) test

disassemble: test
	gdb -batch -ex 'disassemble main' $<
