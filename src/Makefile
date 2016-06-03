.PHONY: all clean disassemble tests

CPPFLAGS = -std=c++14 
CPPFLAGS += -fno-rtti -fno-exceptions
CPPFLAGS += -O2 -Wall -Wextra
CPPFLAGS += -pthread
LDFLAGS =

# Debug
#CPPFLAGS += -g -Og

TESTS_CPP = $(wildcard *.t.cpp)
TESTS_EXEC = $(TESTS_CPP:%.t.cpp=test_%)

all: sparse-mm
tests: $(TESTS_EXEC) givy

test_%: %.t.cpp $(wildcard *.h)
	g++ $(CPPFLAGS) -o $@ $< $(LDFLAGS)

# Main test app
givy: CPPFLAGS += -DASSERT_LEVEL_SAFE
givy: CPPFLAGS += -ffunction-sections
givy: LDFLAGS += -Wl,--gc-sections
givy: main.cpp givy.cpp $(wildcard *.h)
	mpic++ $(CPPFLAGS) -o $@ main.cpp givy.cpp $(LDFLAGS)

sparse-mm: CPPFLAGS += -DASSERT_LEVEL_SAFE
sparse-mm: CPPFLAGS += -ffunction-sections
sparse-mm: LDFLAGS += -Wl,--gc-sections
sparse-mm: LDFLAGS += -lopenblas
sparse-mm: sparse-mm.cpp givy.cpp $(wildcard *.h)
	mpic++ $(CPPFLAGS) -o $@ sparse-mm.cpp givy.cpp $(LDFLAGS)

clean:
	$(RM) $(TESTS_EXEC) givy sparse-mm

