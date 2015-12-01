.PHONY: all clean disassemble

CPPFLAGS = -std=c++14 
CPPFLAGS += -fno-rtti -fno-exceptions
CPPFLAGS += -O3 -Wall -Wextra

FILES = 

all: test_main test_spt test_chain

test_main: region.cpp $(FILES) $(wildcard *.h)
	g++ $(CPPFLAGS) -o $@ $< $(FILES)

test_spt: superpage_tracker.t.cpp $(FILES) $(wildcard *.h)
	g++ $(CPPFLAGS) -o $@ $< $(FILES) -pthread
test_chain: chain.t.cpp $(FILES) $(wildcard *.h)
	g++ $(CPPFLAGS) -o $@ $< $(FILES) -pthread

clean:
	$(RM) $(wildcard test_*)

disassemble: test_main
	gdb -batch -ex 'disassemble main' $<
