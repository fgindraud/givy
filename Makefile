.PHONY: all clean disassemble

CPPFLAGS = -std=c++14 
CPPFLAGS += -fno-rtti -fno-exceptions
CPPFLAGS += -O3 -Wall -Wextra
CPPFLAGS += -pthread
LDFLAGS =

# Debug
#CPPFLAGS += -g -Og

TESTS_CPP = $(wildcard *.t.cpp)
TESTS_EXEC = $(TESTS_CPP:%.t.cpp=test_%)

all: $(TESTS_EXEC) givy name_server

test_%: %.t.cpp $(wildcard *.h)
	g++ $(CPPFLAGS) -o $@ $< $(LDFLAGS)

# Main test app
givy: CPPFLAGS += -DASSERT_LEVEL_NONE
givy: CPPFLAGS += -ffunction-sections
givy: LDFLAGS += -Wl,--gc-sections
givy: LDFLAGS += -lcci
givy: main.cpp givy.cpp $(wildcard *.h)
	g++ $(CPPFLAGS) -o $@ main.cpp givy.cpp $(LDFLAGS)

# Registering server
name_server: LDFLAGS += -lcci
name_server: name_server.cpp $(wildcard *.h)
	g++ $(CPPFLAGS) -o $@ $< $(LDFLAGS)

clean:
	$(RM) $(TESTS_EXEC) givy name_server

