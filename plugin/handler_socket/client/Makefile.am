CXXFLAGS += -fimplicit-templates
AM_INCLUDES= -I$(srcdir)/../libhsclient
bin_PROGRAMS=hsclient 
hsclient_SOURCES= hsclient.cpp
hsclient_LDFLAGS= -static -L../libhsclient -lhsclient
hsclient_CXXFLAGS= $(AM_INCLUDES)

hstest: hstest.o
	$(CXX) $(CXXFLAGS) $(MY_CXXFLAGS) $(LFLAGS) hstest.o \
		-L../libhsclient/.libs -lhsclient $(MYSQL_LIB) \
		-o hstest

hstest.o: hstest.cpp
	$(CXX) $(CXXFLAGS) $(MY_CXXFLAGS) $(MYSQL_INC) $(AM_INCLUDES) \
		-c hstest.cpp

hslongrun: hslongrun.o
	$(CXX) $(CXXFLAGS) $(MY_CXXFLAGS) $(LFLAGS) hslongrun.o \
		-L../libhsclient/.libs -lhsclient $(MYSQL_LIB) \
		-o hslongrun

hslongrun.o: hslongrun.cpp
	$(CXX) $(CXXFLAGS) $(MY_CXXFLAGS) $(MYSQL_INC) $(AM_INCLUDES) \
		-c hslongrun.cpp
