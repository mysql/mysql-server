MYSQL_BUILD_OPTS=--with-comment='MySQL Server (custom)' --with-server-suffix='-custom'
MYSQL_BUILD_CXXFLAGS=-DBIG_JOINS=1 -felide-constructors -fno-rtti -O2
MYSQL_BUILD_CFLAGS=-DBIG_JOINS=1 -O2
MYSQL_BUILD_CC=gcc
MYSQL_BUILD_CXX=gcc
