# Change this to the (absolute) path to your installed mpqc-config script, if needed
MPQCCONFIG = /Users/evaleev/Development/workspace/install/mpqc-gcc/bin/mpqc-config
CXX := $(shell $(MPQCCONFIG) --cxx)
CXXFLAGS := $(shell $(MPQCCONFIG) --cxxflags)
CC := $(shell $(MPQCCONFIG) --cc)
CCFLAGS := $(shell $(MPQCCONFIG) --cflags)
F77 := $(shell $(MPQCCONFIG) --f77)
F90 := $(F77)
FFLAGS := $(shell $(MPQCCONFIG) --f77flags)
F90FLAGS := $(FFLAGS)
CPPFLAGS := $(shell $(MPQCCONFIG) --cppflags)
LIBS := $(shell $(MPQCCONFIG) --libs)
LIBDIR  := $(shell $(MPQCCONFIG) --libdir)
LDFLAGS := $(shell $(MPQCCONFIG) --ldflags)

local: local.o
	$(CXX) $(CXXFLAGS) -o $@ $^ -L$(LIBDIR) $(LDFLAGS) $(LIBS)

clean:
	-rm -f local local.o
