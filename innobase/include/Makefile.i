# Makefile included in Makefile.am in every subdirectory

libsdir = ../libs

INCLUDES =		-I../../include -I../include

CFLAGS= -g -O2 -DDEBUG_OFF

# Don't update the files from bitkeeper
%::SCCS/s.%
