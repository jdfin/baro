
CLANG_FORMAT = clang-format-3.7

CXXFLAGS += -std=gnu++11 -O2
CXXFLAGS += -I$(GTEST_ROOT)/include

#LDLIBS += -lpthread
LDPATH += -L$(GMOCK_ROOT)/gtest

default: ms5611_log ms5611_test

ms5611_log: ms5611.o ms5611_log.o
	$(LINK.cpp) -o $@ $^

ms5611_test: ms5611.o ms5611_test.o
	$(LINK.cpp) -o $@ $^ $(LDPATH) -lgtest -lpthread

format:
	clang-format-3.7 -i -style=file *.h *.cpp

clean:
	rm -f *.o ms5611_log ms5611_test
