# Makefile included in Makefile.am in every subdirectory

INCLUDES =		-I$(srcdir)/../include -I$(srcdir)/../../include -I../../include

# Don't update the files from bitkeeper
%::SCCS/s.%
