
noinst_LIBRARIES = libeditline.a

libeditline_a_SOURCES = complete.c editline.c sysunix.c

INCLUDES = -I$(top_srcdir)/include -I$(top_srcdir)/ndb/include
DEFS = -DANSI_ARROWS -DHAVE_TCGETATTR -DSYS_UNIX

# Don't update the files from bitkeeper
%::SCCS/s.%
