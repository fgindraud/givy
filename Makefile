.PHONY: all disassemble

all: test

test: region.cpp
	g++ -std=c++11 -O3 -Wall -o $@ $<

disassemble: test
	gdb -batch -ex 'disassemble main' $<
