.PHONY: all clean disassemble

CPPFLAGS = -std=c++14 -O3 -Wall

all: test_chain

test_chain: chain.cpp
	g++ $(CPPFLAGS) -o $@ $<

clean:
	$(RM) $(wildcard test_*)

disassemble: test_main
	gdb -batch -ex 'disassemble main' $<
