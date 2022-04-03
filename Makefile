CXX = g++
CC = gcc
DEBUG = true
ifeq ($(DEBUG), true)
	CXXFLAGS = -Wall -std=c++17 -g -I ./include/
else
	CXXFLAGS = -Wall -std=c++17 -I ./include/ -O3
endif
TARGETS = report_absent write_record today_info sess_req
DLLS = sqlite3mc_x64.dll

.PHONY: all
all: $(TARGETS)

report_absent: report_absent.o
	$(CXX) $^ $(DLLS) -o $@

report_absent.o: report_absent.cpp dbman.h

write_record: write_record.o
	$(CXX) $^ $(DLLS) -o $@

write_record.o: write_record.cpp dbman.h

today_info: today_info.o
	$(CXX) $^ $(DLLS) -o $@

today_info.o: today_info.cpp dbman.h

sess_req: sess_req.o
	$(CXX) $^ $(DLLS) -o $@

sess_req.o: sess_req.cpp dbman.h

.PHONY: clean
clean:
	rm *.exe *.o -f