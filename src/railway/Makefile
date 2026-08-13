# Railway (ETCS-3) scenario for Figure 10.
#
# The scenario lives in ns-3's scratch/ tree, but ns-3's own build system
# cannot be used here: railway-ns3/cmake-cache/CMakeCache.txt records absolute
# paths from the machine the tree was built on, so `./ns3 configure` would
# discard the prebuilt libraries and rebuild all of ns-3 from source.
#
# Instead we compile the scenario directly against the libraries and headers
# that are already in railway-ns3/build/.  Two settings that Figure 10 needs
# were compiled out of the original and are set here:
#   ENABLE_TRACK_LOG=1  write track_logs/train-<region>.csv  (was 0)
#   END_TIME=400        seconds of simulated time            (was 60)
#   PING_TIMEOUT=200000 D_t/o in us, the paper's value       (was 500000)

NS3_ROOT ?= $(abspath ../../../railway-ns3)
NS3_INC  := $(NS3_ROOT)/build/include
NS3_LIB  := $(NS3_ROOT)/build/lib

END_TIME ?= 400
# D_t/o in microseconds.  The paper's railway case study uses 200 ms; the tree
# ships 500 ms.  See src/railway/simulation/defs.h.
PING_TIMEOUT ?= 200000
# 0 = tree default; 1/2/3 = Figure 10 stable / manual-signalling / HEIMDALLR
SCENARIO ?= 0

# ns-3.38's Singleton<T> does not compile as C++20; the tree was built at C++17.
CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -w
DEFINES  := -DENABLE_TRACK_LOG=1 -DEND_TIME=$(END_TIME) \
            -DFIG10_SCENARIO=$(SCENARIO) -DPING_TIMEOUT=$(PING_TIMEOUT)

SRCS := $(wildcard simulation/*.cc)
LIBS := $(patsubst $(NS3_LIB)/lib%.so,-l%,$(wildcard $(NS3_LIB)/libns3.38-*.so))

sim: $(SRCS)
	@test -d "$(NS3_INC)/ns3" || { \
	  echo "ns-3 headers not found at $(NS3_INC)."; \
	  echo "Set NS3_ROOT to a railway-ns3 checkout with a populated build/."; \
	  exit 1; }
	$(CXX) $(CXXFLAGS) $(DEFINES) -o $@ $(SRCS) \
	  -I$(NS3_INC) -L$(NS3_LIB) $(LIBS) -lcrypto -lssl \
	  -Wl,-rpath,$(NS3_LIB)

clean:
	rm -f sim

.PHONY: clean
