# Makefile included in Makefile.am in every subdirectory

INCLUDES =		-I$(top_srcdir)/include -I$(top_srcdir)/../../include

# Don't update the files from bitkeeper
%::SCCS/s.%
