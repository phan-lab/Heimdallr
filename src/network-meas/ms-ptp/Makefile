# Compiler
CXX = g++

# Compiler flags
CXXFLAGS = -std=c++17 -Wall -Wextra -O3

# Linker flags
LDFLAGS = -lsodium -pthread

# Directories 
SUBDIRS = keygen

# Target executables
TARGETS = grand-master follower

# Source files
SRCS = grand-master.cc follower.cc profiler.cc

# Object files
OBJS = $(SRCS:.cc=.o)

# Default target
all: $(TARGETS) $(SUBDIRS)
	@for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir; \
	done

# Rule to build grand-master
grand-master: grand-master.o perf.o
	$(CXX) $(CXXFLAGS) -o grand-master grand-master.o perf.o $(LDFLAGS)

# Rule to build follower
follower: follower.o perf.o
	$(CXX) $(CXXFLAGS) -o follower follower.o perf.o $(LDFLAGS)

# Compile source files into object files
%.o: %.cc
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Clean up build files
clean:
	rm -f $(OBJS) $(TARGETS)
	@for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir clean; \
	done

# Phony targets (not actual files)
.PHONY: all clean $(SUBDIRS)