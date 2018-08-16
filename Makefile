CXX = g++
CXXFLAGS = -std=c++11 -Wall -lGLU
LINK.o = $(CXX)

all: Main

Main: Main.o Simulator.o Utilities.o

clean:
	rm -f Main *.o *~
