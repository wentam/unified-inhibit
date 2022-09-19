prefix = /usr/local/
INCLUDE = -Iinclude/ -I. $(shell pkgconf dbus-1 --cflags)
CXXFLAGS = ${INCLUDE} -pipe -std=c++20 -pedantic -Wextra -Wall -Wno-maybe-uninitialized -Wno-unused-function -Wno-sign-compare -Wno-unused-parameter -Wunused-result -fcoroutines
LINK = -ldbus-1
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
	install -m=0755 build/uinhibitd ${DESTDIR}${prefix}/bin/uinhibitd

.PHONY:clean
clean:
	rm -rf build/

.PHONY:test
test: build/test
	@echo "---- Begin tests ----"
	@build/test

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
