.PHONY: all clean disassemble

FILES = superpage_tracker.cpp

all: test_main test_spt

test_main: region.cpp $(FILES) $(wildcard *.h)
	g++ -std=c++14 -O3 -Wall -o $@ $< $(FILES)

test_spt: test_spt.cpp $(FILES) $(wildcard *.h)
	g++ -std=c++14 -O3 -Wall -o $@ $< $(FILES) -pthread

clean:
	$(RM) $(wildcard test_*)

disassemble: test_main
	gdb -batch -ex 'disassemble main' $<
