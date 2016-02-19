# To build this project, four external resources are needed:
#
# - the (Boost::)ASIO header-only library (Boost is not needed)
# - the SQLiteCpp library ( https://github.com/SRombauts/SQLiteCpp )
# - the sqlite3 library
# - the bzip2 library

ASIO := ~/asio-1.10.6/include/
SQLITECPP := ./sqlitecpp/

appname := cppcddbd

CXX := g++
CXXFLAGS := -Wall -O2 -std=c++14 -pthread -I $(ASIO)
LDFLAGS := -lpthread -lsqlite3 -lbz2

srcfiles := $(shell find . -maxdepth 1 -name "*.cpp")
objects  := $(patsubst %.cpp, %.o, $(srcfiles))
sqllib   := $(shell find $(SQLITECPP) -maxdepth 1 -name "*.o")

all: $(appname)

$(appname): $(objects)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $(appname) $(objects) $(sqllib) $(LDLIBS)
	
depend: .depend
	
.depend: $(srcfiles)
	rm -f ./.depend
	$(CXX) $(CXXFLAGS) -MM $^>>./.depend;
	
clean:
	rm -f $(objects)
	
dist-clean: clean
	rm -f *~ .depend
	
include .depend
