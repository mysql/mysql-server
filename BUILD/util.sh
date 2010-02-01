# MariaDB SQL server.
# Copyright (C) 2010 Kristian Nielsen and Monty Program AB
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
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

# Setting cpu options.
get_cpuopt () {
    case "$(uname -o)" in
      *Linux*)
	case "$(gcc -dumpmachine)" in
          x86_64-*)
                # gcc barfs on -march=... on x64
                CPUOPT="-m64 -mtune=generic"
                ;;
          *)
                # we'd use i586 to not trip up mobile/lowpower devices
                CPUOPT="-m32 -march=i586 -mtune=generic"
                ;;
	esac
	;;
      *Solaris*)
	# ToDo: handle 32-bit build? For now default to 64-bit.
	CPUOPT="-D__sun -m64 -mtune=athlon64"
	;;
    esac
    return 0
}

# Default to a parallel build, but only if AM_MAKEFLAGS is not set.
# (So buildbots can easily disable this behaviour if required.)
get_make_parallel_flag () {
        if test -z "$AM_MAKEFLAGS"
        then
                AM_MAKEFLAGS="-j 6"
        fi
	return 0
}
