INCLUDE = -Iinclude/ -I.
LIBINCLUDE = -Iinclude/lib/
.PHONY: docs multi build clean cleandocs lib sphinx doxygen depend configReader
CXX = g++ -g -pipe -O2 -std=c++20 -pedantic -Wextra -Wall -Wno-maybe-uninitialized -Wno-unused-function -Wno-sign-compare -Wno-unused-parameter -Wunused-result -fcoroutines

rwildcard=$(wildcard $1$2) $(foreach d,$(wildcard $1*),$(call rwildcard,$d/,$2))

LIBSRCS = $(call rwildcard,src/,*.cpp)
LIBOBJS = $(LIBSRCS:src/%.cpp=build/%.o)
LIBDEPS = $(LIBOBJS:%.o=%.d)

#$(info -----------------------------------------------)
#$(info LIBSRCS - ${LIBSRCS})
#$(info -----------------------------------------------)
#$(info LIBOBJS - ${LIBOBJS})
#$(info -----------------------------------------------)
#$(info LIBDEPS - ${LIBDEPS})
#$(info -----------------------------------------------)

build: build/uinhibitd

build/:
	mkdir -p build

build/uinhibitd: build/ $(LIBOBJS)
	$(CXX) $(INCLUDE) $(LIBINCLUDE) -ldbus-1 -o build/uinhibitd $(LIBOBJS)

-include $(LIBDEPS)

build/%.o: src/%.cpp
	@mkdir -p $(@D)
	$(CXX) -fPIC -MMD -c $(INCLUDE) $(LIBINCLUDE) $< -o $@

clean:
	rm -rf build/
