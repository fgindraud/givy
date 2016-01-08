.PHONY: all clean disassemble

CPPFLAGS = -std=c++14 
CPPFLAGS += -fno-rtti -fno-exceptions
CPPFLAGS += -O3 -Wall -Wextra
CPPFLAGS += -pthread
LDFLAGS = 

# Options to remove unused symbols (inlined...)
FLAGS_OPT = -ffunction-sections -Wl,--gc-sections
FLAGS_OPT += -fvisibility-inlines-hidden # for gcc only, tell him to ditch inline symbols as dll exports

# Debug
#CPPFLAGS += -g -Og

TESTS_CPP = $(wildcard *.t.cpp)
TESTS_EXEC = $(TESTS_CPP:%.t.cpp=test_%)

all: $(TESTS_EXEC) givy

test_%: %.t.cpp $(wildcard *.h)
	g++ $(CPPFLAGS) -o $@ $< $(LDFLAGS)

givy: main.cpp $(wildcard *.h)
	g++ $(CPPFLAGS) $(FLAGS_OPT) -o $@ $< $(LDFLAGS)

clean:
	$(RM) $(TESTS_EXEC) givy

