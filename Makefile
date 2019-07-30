#  Copyright (C) 2016-2018 Bartek Dyda <bartekdyda@protonmail.com>.
# 
#  This file is part of Kropla.
#
# usage
#  $ make [NDEBUG=1] [SGF=1] [CPU=native] [SPEED_TEST=1]
# or 
#  $ make clean

override CXXFLAGS += -std=c++11
DEFS =
LIBS += -pthread
INCL=-I../boost/boost_1_63_0
SUFFIX =


ifdef NDEBUG
  override CXXFLAGS += -s -O3
  override DEFS += -DNDEBUG
  LDFLAGS  += -s
  SUFFIX :=$(strip $(SUFFIX))-opt
else
  override CXXFLAGS += -g -O0
  LDFLAGS  += -g
endif

ifdef CPU
  override CXXFLAGS += -march=$(CPU)
endif

ifdef SGF
  override DEFS += -DDEBUG_SGF
  SUFFIX :=$(strip $(SUFFIX))-sgf
endif

ifdef SPEED_TEST
  override DEFS += -DSPEED_TEST
  SUFFIX :=$(strip $(SUFFIX))-speed
endif


objects = board$(SUFFIX).o sgf$(SUFFIX).o game$(SUFFIX).o command$(SUFFIX).o patterns$(SUFFIX).o influence$(SUFFIX).o
kropla = kropla$(SUFFIX)

$(kropla) : $(objects)
	$(CXX) $(LDFLAGS) $(DEFS) $(INCL) -o $(kropla) $(objects) $(LIBS)

board$(SUFFIX).o: board.cc board.h connections_tab02.cc connections_tab03_simple.cc
	$(CXX) -c $(CXXFLAGS) $(DEFS) $(INCL) board.cc -o board$(SUFFIX).o

sgf$(SUFFIX).o: sgf.cc sgf.h
	$(CXX) -c $(CXXFLAGS) $(DEFS) $(INCL) sgf.cc -o sgf$(SUFFIX).o

command$(SUFFIX).o: command.cc command.h board.h
	$(CXX) -c $(CXXFLAGS) $(DEFS) $(INCL) command.cc -o command$(SUFFIX).o

patterns$(SUFFIX).o: patterns.cc patterns.h board.h
	$(CXX) -c $(CXXFLAGS) $(DEFS) $(INCL) patterns.cc -o patterns$(SUFFIX).o

influence$(SUFFIX).o: influence.cc influence.h board.h
	$(CXX) -c $(CXXFLAGS) $(DEFS) $(INCL) influence.cc -o influence$(SUFFIX).o

game$(SUFFIX).o: game.cc sgf.h board.h connections_tab02.cc connections_tab03_simple.cc
	$(CXX) -c $(CXXFLAGS) $(DEFS) $(INCL) game.cc -o game$(SUFFIX).o


.PHONY : clean
clean :
	-rm $(kropla) $(objects)

