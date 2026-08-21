CXX ?= clang++
CXXFLAGS ?= -std=c++20 -O3 -march=native -funroll-loops -fno-math-errno -Wall -Wextra -Wno-unused-parameter
LDFLAGS ?= -pthread

all: fish

fish: src/main.cpp src/*.hpp
	$(CXX) $(CXXFLAGS) src/main.cpp -o fish $(LDFLAGS)

clean:
	rm -f fish
