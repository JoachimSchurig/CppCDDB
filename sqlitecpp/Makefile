
appname := none

CXX := g++
CXXFLAGS := -Wall -O2 -std=c++14 -pthread
LDFLAGS := -lpthread

srcfiles := $(shell find . -maxdepth 1 -name "*.cpp")
objects  := $(patsubst %.cpp, %.o, $(srcfiles))

all: lib

lib: $(objects)

demo: $(appname)

$(appname): $(objects)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $(appname) ./demo.o $(objects) $(LDLIBS)
	
depend: .depend
	
.depend: $(srcfiles)
	rm -f ./.depend
	$(CXX) $(CXXFLAGS) -MM $^>>./.depend;
	
clean:
	rm -f $(objects)
	
dist-clean: clean
	rm -f *~ .depend
	
include .depend
