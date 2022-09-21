# - User settings -
prefix = /usr/local/
X11 ?=1
# -----------------

INCLUDE = -Iinclude/ -I. $(shell pkgconf dbus-1 --cflags)
CXXFLAGS = ${INCLUDE} -pipe -std=c++20 -pedantic -Wextra -Wall -Wno-maybe-uninitialized -Wno-unused-function -Wno-sign-compare -Wno-unused-parameter -Wunused-result -fcoroutines
LINK = -ldbus-1

ifeq "$(X11)" "1"
LINK += -lX11 -lXext
CXXFLAGS += -DBUILDFLAG_X11
endif

CXX = g++

rwildcard=$(wildcard $1$2) $(foreach d,$(wildcard $1*),$(call rwildcard,$d/,$2))

SRCS = $(call rwildcard,src/,*.cpp)
OBJS = $(SRCS:src/%.cpp=build/%.o)
DEPS = $(OBJS:%.o=%.d)
DEPS += $(call rwildcard,build/,*.d)

.PHONY:default
default: release

.PHONY:release
release: CXXFLAGS += -Os
release: build/uinhibitd
	strip build/uinhibitd

.PHONY:debug
debug: CXXFLAGS += -g -DDEBUG -Og
debug: build/uinhibitd

.PHONY:install
install:
	mkdir -p ${DESTDIR}${prefix}/bin
	mkdir -p ${DESTDIR}${prefix}/share/man/man1
	install -m=0755 build/uinhibitd ${DESTDIR}${prefix}/bin/uinhibitd
	install doc/uinhibitd.1.roff ${DESTDIR}${prefix}/share/man/man1/uinhibitd.1

.PHONY:clean
clean:
	rm -rf build/

.PHONY:test
test: build/test
	@echo "---- Begin tests ----"
	@build/test

# For development: .roff file should be in-repo such that users don't need scdoc to build.
.PHONY:doc
doc: doc/uinhibitd.1.roff
	man doc/uinhibitd.1.roff

build/:
	mkdir -p build

build/uinhibitd: build/ $(OBJS)
	$(CXX) $(CXXFLAGS) $(LINK) -o $@ $(OBJS)

-include $(DEPS)

build/%.o: src/%.cpp
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) -fPIC -MMD -c $< -o $@

build/test: CXXFLAGS += -g -DDEBUG -Og
build/test: test/test.cpp $(filter-out build/main.o,$(OBJS))
	$(CXX) $(CXXFLAGS) $< $(LINK) -MMD -o $@ $(filter-out build/main.o,$(OBJS))

doc/uinhibitd.1.roff: doc/uinhibitd.1.scd
	SOURCE_DATE_EPOCH=$(shell date +%s) scdoc < doc/uinhibitd.1.scd > doc/uinhibitd.1.roff
