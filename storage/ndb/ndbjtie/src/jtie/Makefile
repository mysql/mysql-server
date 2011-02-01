# Copyright 2009 Sun Microsystems, Inc.
#  All rights reserved. Use is subject to license terms.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; version 2 of the License.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

#------------------------------------------------------------------------------

# read the generic settings
include	../../Makefile.defaults
include	../../env.properties

#------------------------------------------------------------------------------

  JAVA_SRC	= .

# preprocessing defines:
  DDEFINES 	= 

  DELIVERABLES	= $(JAVA_SRC)/jtie/Wrapper.class

  CLEAN		= core *.o *.class dummy.cpp

  CLOBBER	= $(DELIVERABLES) *.dSYM

  DISTCLEAN	= *~ *% */*~

  SRCS		= *.cpp

# currently, not needed:
#  OBJECTS	= $(SRC:.c=.o)
#  OBJECTS	+= $(SRC:.cc=.o)
#  OBJECTS	+= $(SRC:.cpp=.o)

  INCLUDES	= -I. -I.. $(JAVA_INCLUDEOPTS)

  CLASSPATH	= "."

# for library dependencies, LOADLIBES and LDLIBS have been used historically
# this convention is used sometimes: use
#   LDLIBS for project-wide libs (like -lc or -lm)
#   LOADLIBES for libs for the individual case
# check with non-gnu makes: use of LOADLIBES, LDLIBS
# CLOCK_GETTIME requires -lrt
  LDLIBS	=
#  LDLIBS	= -lrt

#------------------------------------------------------------------------------

.PHONY:	all clean clobber depend dep

all:	$(DELIVERABLES)

$(JAVA_SRC)/jtie/Wrapper.class:	$(JAVA_SRC)/jtie/Wrapper.java
	$(JAVAC) $(JAVACFLAGS) -classpath $(CLASSPATH) $<

dummy.cpp:
	touch $@

dep depend:	dummy.cpp

#------------------------------------------------------------------------------

.depend:
	touch $@

# read local dependencies
-include	.depend

#------------------------------------------------------------------------------
# DO NOT DELETE
