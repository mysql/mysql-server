CXXFLAGS += -fimplicit-templates
AM_INCLUDES= -I$(srcdir)/../libhsclient
bin_PROGRAMS=hsclient 
hsclient_SOURCES= hsclient.cpp
hsclient_LDFLAGS= -static -L../libhsclient -lhsclient
hsclient_CXXFLAGS= $(AM_INCLUDES)

hstest: hstest.o
	$(CXX) $(CXXFLAGS) $(LFLAGS) hstest.o \
		-L../libhsclient/.libs -lhsclient $(MYSQL_LIB) -o hstest

hstest.o: hstest.cpp
	$(CXX) $(CXXFLAGS) $(MYSQL_INC) $(AM_INCLUDES) -c hstest.cpp

