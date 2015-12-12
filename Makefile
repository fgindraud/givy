.PHONY: all clean disassemble

CPPFLAGS = -std=c++14 
CPPFLAGS += -fno-rtti -fno-exceptions
CPPFLAGS += -O3 -Wall -Wextra
CPPFLAGS += -pthread
#CPPFLAGS += -g -Og

TESTS_CPP = $(wildcard *.t.cpp)
TESTS_EXEC = $(TESTS_CPP:%.t.cpp=test_%)

all: $(TESTS_EXEC) givy

test_%: %.t.cpp $(wildcard *.h)
	g++ $(CPPFLAGS) -o $@ $<

givy: main.cpp $(wildcard *.h)
	g++ $(CPPFLAGS) -o $@ $<

clean:
	$(RM) $(TESTS_EXEC) givy

