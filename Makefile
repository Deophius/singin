CXX = g++
CC = gcc
CXXFLAGS = -Wall -std=c++17 -g -I ./include/
TARGETS = test_dbman report_absent
DLLS = sqlite3mc_x64.dll

.PHONY: all
all: $(TARGETS)

test_dbman: test_dbman.o
	$(CXX) $^ $(DLLS) -o $@

test_dbman.o: test_dbman.cpp dbman.h

report_absent: report_absent.o
	$(CXX) $^ $(DLLS) -o $@

report_absent.o: report_absent.cpp dbman.h
