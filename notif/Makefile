CXX = g++
CC = gcc
DEBUG = true
ifeq ($(DEBUG), true)
	CXXFLAGS = -Wall -std=c++17 -g -I ../include/ -I ..
else
	CXXFLAGS = -Wall -std=c++17 -I ../include/ -I .. -O3
endif

TARGETS = machine_id in_signin
HEADERS = ../dbman.h
LIB = ../sqlite3mc_x64.dll

.PHONY: all
all: $(TARGETS)

machine_id: machine_id.o
	$(CXX) $^ $(LIB) -o $@

machine_id.o: machine_id.cpp $(HEADERS)

in_signin: in_signin.o
	$(CXX) $^ $(LIB) -o $@

in_signin.o: in_signin.cpp $(HEADERS)