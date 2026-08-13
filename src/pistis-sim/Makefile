CXX ?= g++
CXXFLAGS ?= -std=c++17 -O3 -march=native -Wall -Wextra
LDFLAGS = -pthread

all: sim

sim: sim.o node.o
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.cc node.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f sim sim.o node.o

.PHONY: all clean
