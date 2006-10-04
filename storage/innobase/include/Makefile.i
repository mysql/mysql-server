# Makefile included in Makefile.am in every subdirectory

INCLUDES =              -I$(top_srcdir)/include -I$(top_builddir)/include \
			-I$(top_srcdir)/regex \
			-I$(top_srcdir)/storage/innobase/include \
			-I$(top_srcdir)/sql \
                        -I$(srcdir)

# Don't update the files from bitkeeper
%::SCCS/s.%
