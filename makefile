prefix = /usr/local/
INCLUDE = -Iinclude/ -I. $(shell pkgconf dbus-1 --cflags)
CXXFLAGS = ${INCLUDE} -pipe -std=c++20 -pedantic -Wextra -Wall -Wno-maybe-uninitialized -Wno-unused-function -Wno-sign-compare -Wno-unused-parameter -Wunused-result -fcoroutines
CXX = g++

rwildcard=$(wildcard $1$2) $(foreach d,$(wildcard $1*),$(call rwildcard,$d/,$2))

SRCS = $(call rwildcard,src/,*.cpp)
OBJS = $(SRCS:src/%.cpp=build/%.o)
DEPS = $(OBJS:%.o=%.d)

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

build/:
	mkdir -p build

build/uinhibitd: build/ $(OBJS)
	$(CXX) $(CXXFLAGS) -ldbus-1 -o $@ $(OBJS)

-include $(DEPS)

build/%.o: src/%.cpp
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) -fPIC -MMD -c $< -o $@
