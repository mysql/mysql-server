# Makefile included in Makefile.am in every subdirectory

libsdir = ../libs

INCLUDES =		-I../../include -I../include

# Don't update the files from bitkeeper
%::SCCS/s.%
