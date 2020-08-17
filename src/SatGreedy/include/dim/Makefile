CXX = g++
CXXFLAGS = -g -O2 -std=c++11 -Wall

all: do

do: main.cpp dim.cpp
	$(CXX) $(CXXFLAGS) -o $@ $^

.PHONY: clean

clean:
	rm -f do
