# Compiler
CXX = g++

# Compiler flags
CXXFLAGS = -std=c++17 -Wall -Wextra -O3 -g

# Linker flags
LDFLAGS = -lsodium -pthread

# Directories 
SUBDIRS = keygen

# Target executables
TARGETS = upstream 

# Source files
SRCS = upstream.cc  profiler.cc

# Object files
OBJS = $(SRCS:.cc=.o)

# Default target
all: $(TARGETS) $(SUBDIRS)
	@for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir; \
	done

# Rule to build upstream
upstream: upstream.o perf.o node.o
	$(CXX) $(CXXFLAGS) -o upstream upstream.o perf.o node.o $(LDFLAGS)


# Compile source files into object files
%.o: %.cc %.h common.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Clean up build files
clean:
	rm -f $(OBJS) $(TARGETS)
	@for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir clean; \
	done

# Phony targets (not actual files)
.PHONY: all clean $(SUBDIRS)