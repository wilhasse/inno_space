CXX = g++
CXXFLAGS = -Wall -W -DNDEBUG -g -O2 -std=c++11
OBJECT = inno
SRC_DIR = src

LIB_PATH = -L./
LIBS =

INCLUDE_PATH = -I./ \
							 -I./include/ \

.PHONY: all clean test

BASE_BOJS := $(wildcard $(SRC_DIR)/*.cc)
BASE_BOJS += $(wildcard $(SRC_DIR)/*.c)
OBJS = $(patsubst %.cc,%.o,$(BASE_BOJS))
TEST_SRCS := $(wildcard tests/*.cpp)
TEST_OBJS := $(patsubst %.cpp,%.o,$(TEST_SRCS))
SRC_TEST_OBJS := src/mach_data.o src/zipdecompress_stub.o src/parse_fil_header.o

test: unit_tests

unit_tests: $(TEST_OBJS) $(SRC_TEST_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(INCLUDE_PATH) $(LIB_PATH) $(LIBS) -lz

all: $(OBJECT)
	rm $(SRC_DIR)/*.o

# The top-level link rule (final link):
$(OBJECT): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(INCLUDE_PATH) $(LIB_PATH) $(LIBS) -lz

# Compile rule (object files):
%.o : %.cc
	$(CXX) $(CXXFLAGS) -c $< -o $@ $(INCLUDE_PATH)
%.o : %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@ $(INCLUDE_PATH)

clean:
	rm -rf $(OBJECT) ./a.out
	rm -rf $(SRC_DIR)/*.o
	rm -f unit_tests tests/*.o src/mach_data.o src/zipdecompress_stub.o src/parse_fil_header.o
