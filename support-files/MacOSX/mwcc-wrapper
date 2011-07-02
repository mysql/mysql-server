#!/bin/sh

# Copyright (C) 2005 MySQL AB
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
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

if [ -z "$CWINSTALL" ] ; then
    echo "ERROR: You need to source 'mwvars' to set CWINSTALL and other variables"
    exit 1
fi

if [ `expr "$MWMacOSXPPCLibraryFiles" : ".*BSD.*"` = 0 ] ; then
    echo "ERROR: You need to source 'mwvars' with the 'bsd' argument"
    exit 1
fi

# ==============================================================================

# Extra options that don't change

PREOPTS="-D__SCHAR_MAX__=127 -D__CHAR_BIT__=8 -ext o -gccinc"
PREOPTS="$PREOPTS -wchar_t on -bool on -relax_pointers -align power_gcc"
PREOPTS="$PREOPTS -stabs all -fno-handle-exceptions -Cpp_exceptions off"

# ==============================================================================

# We want the "PPC Specific" directory to be last, before the source
# file. It is to work around a CodeWarrior/Apple bug, that we need a
# Metrowersk header even though we have configured CodeWarrior to use
# the BSD headers. But not to conflict, the directory has to be last.

# FIXME this will probably break if one path contains space characters

PREARGS=""
for i in $* ; do
    case "$i" in
	-bind_at_load)
	# This is a flag some version of libtool adds, when the host
	# is "*darwin*". It doesn't check that it is gcc.
	# FIXME add some flag?!
	;;
	*.c|*.cc|*.cpp)
	break
	;;
	*)
	PREARGS="$PREARGS $1"
	;;
    esac
    shift
done
#echo "mwcc $PREOPTS $PREARGS -I\"$CWINSTALL/MacOS X Support/Headers/PPC Specific\" $*"
exec mwcc $PREOPTS $PREARGS -I"$CWINSTALL/MacOS X Support/Headers/PPC Specific" $*
