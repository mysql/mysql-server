#!/bin/sh

# Copyright (c) 2005, 2010, Oracle and/or its affiliates. All rights reserved.
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

#
# Check cpu of current machine and find the
# best compiler optimization flags for gcc
# Will return result in:
# cpu_arg        : Type of CPU
# low_cpu_arg    : Type of CPU used up until GCC v3.3
# check_cpu_args : Arguments for GCC compiler settings
#

check_compiler_cpu_flags () {
  # different compiler versions have different option names
  # for CPU specific command line options
  if test -z "$CC" ; then
    cc="gcc";
  else
    cc=$CC
  fi

  # check if compiler is gcc and dump its version
  cc_verno=`$cc -dumpversion 2>/dev/null`
  if test "x$?" = "x0" ; then
    set -- `echo $cc_verno | tr '.' ' '`
    cc_ver="GCC"
    cc_major=$1
    cc_minor=$2
    cc_patch=$3
    cc_comp=`expr $cc_major '*' 100 '+' $cc_minor`
  fi

  case "$cc_ver--$cc_verno" in
    *GCC*)
        # different gcc backends (and versions) have different CPU flags
        case `gcc -dumpmachine` in
          i?86-* | x86_64-*)
	    if test "$cc_comp" -lt 304 ; then
              check_cpu_cflags="-mcpu=${low_cpu_arg}"
            elif test "$cc_comp" -ge 402 ; then
              check_cpu_cflags="-mtune=native"
            else
              check_cpu_cflags="-mtune=${cpu_arg}"
            fi
            ;;
          ppc-*)
              check_cpu_cflags="-mcpu=${cpu_arg} -mtune=${cpu_arg}"
            ;;
          *)
            check_cpu_cflags=""
            return
            ;;
        esac
      ;;
    2.95.*)
      # GCC 2.95 doesn't expose its name in --version output
      check_cpu_cflags="-m${cpu_arg}"
      ;;
    *)
      check_cpu_cflags=""
      return
      ;;
  esac

  # now we check whether the compiler really understands the cpu type
  touch __test.c

  while [ "$cpu_arg" ] ; do
    printf "testing $cpu_arg ... " >&2

    # compile check
    eval "$cc -c $check_cpu_cflags __test.c" 2>/dev/null
    if test "x$?" = "x0" ; then
      echo ok >&2
      break;
    fi

    echo failed >&2
    check_cpu_cflags=""
    break;
  done
  rm __test.*
  return 0
}

check_cpu () {
  CPUINFO=/proc/cpuinfo
  if test -n "$TEST_CPUINFO" ; then
    CPUINFO=$TEST_CPUINFO
  fi
  if test -r "$CPUINFO" -a "$CPUINFO" != " " ; then
    # on Linux (and others?) we can get detailed CPU information out of /proc
    cpuinfo="cat $CPUINFO"

    # detect CPU architecture
    cpu_arch=`$cpuinfo | grep 'arch' | cut -d ':' -f 2 | cut -d ' ' -f 2 | head -1`

    # detect CPU family
    cpu_family=`$cpuinfo | grep 'family' | cut -d ':' -f 2 | cut -d ' ' -f 2 | head -1`
    if test -z "$cpu_family" ; then
      cpu_family=`$cpuinfo | grep 'cpu' | cut -d ':' -f 2 | cut -d ' ' -f 2 | head -1`
    fi

    # detect CPU vendor and model
    cpu_vendor=`$cpuinfo | grep 'vendor_id' | cut -d ':' -f 2 | cut -d ' ' -f 2 | head -1`
    model_name=`$cpuinfo | grep 'model name' | cut -d ':' -f 2 | head -1`
    if test -z "$model_name" ; then
      model_name=`$cpuinfo | grep 'cpu model' | cut -d ':' -f 2 | head -1`
    fi

    # fallback: get CPU model from uname output
    if test -z "$model_name" ; then
      model_name=`uname -m`
    fi

    # parse CPU flags
    for flag in `$cpuinfo | grep '^flags' | sed -e 's/^flags.*: //' -e 's/[^a-zA-Z0-9_ ]/_/g'`; do 
      eval cpu_flag_$flag=yes
    done
  else
    # Fallback when there is no /proc/cpuinfo
    CPUINFO=" "
    case "`uname -s`" in
      FreeBSD|OpenBSD)
        cpu_family=`uname -m`;
        model_name=`sysctl -n hw.model`
        ;;
      Darwin)
        cpu_family=`sysctl -n machdep.cpu.vendor`
        model_name=`sysctl -n machdep.cpu.brand_string`
        if [ -z "$cpu_family" -o -z "$model_name" ]
        then  
          cpu_family=`uname -p`
          model_name=`machine`
        fi  
        ;;
      *)
        cpu_family=`uname -p`;
        model_name=`uname -m`;
        ;;
    esac
  fi

  # detect CPU shortname as used by gcc options 
  # this list is not complete, feel free to add further entries
  cpu_arg=""
  low_cpu_arg=""
  case "$cpu_vendor--$cpu_family--$model_name--$spu_arch" in
    # DEC Alpha
    *Alpha*EV6*)
      cpu_arg="ev6";
      ;;
    #Core 2 Duo  
    *Intel*Core\(TM\)2*)
      cpu_arg="nocona"
      core2="yes"
      ;;
    # Intel ia32
    *Intel*Core*|*X[eE][oO][nN]*)
      # a Xeon is just another pentium4 ...
      # ... unless it has the "lm" (long-mode) flag set, 
      # in that case it's a Xeon with EM64T support
      # If SSE3 support exists it is a Core2 Duo or newer
      # So is Intel Core.
      if [ -z "$cpu_flag_lm" ]; then
        cpu_arg="pentium4"
      else
        cpu_arg="nocona"
      fi
      if test -z "$cpu_flag_ssse3" ; then
        core2="no"
      else
        core2="yes"
      fi
      ;;
    *Pentium*4*Mobile*)
      cpu_arg="pentium4m"
      ;;
    *Pentium\(R\)*\ M*)
      cpu_arg="pentium-m"
      low_cpu_arg="pentium3"
      ;;
    *Pentium\(R\)*\ D*)
      cpu_arg="prescott"
      ;;
    *Pentium*4*)
      cpu_arg="pentium4"
      ;;
    *Pentium*III*Mobile*)
      cpu_arg="pentium3m"
      ;;
    *Pentium*III*)
      cpu_arg="pentium3"
      ;;
    *Pentium*M*pro*)
      cpu_arg="pentium-m"
    ;;
    *Celeron\(R\)*\ M*)
      cpu_arg="pentium-m"
      ;;
    *Celeron*Coppermine*)
      cpu_arg="pentium3"
      ;;
    *Celeron\(R\)*)
      cpu_arg="pentium4"
      ;;
    *Celeron*)
      cpu_arg="pentium2"
      ;;
    *Atom*)
      cpu_arg="prescott"
      ;;
    *GenuineIntel*)
      cpu_arg="pentium"
      ;;
    *Turion*)
      cpu_arg="athlon64"
      ;;
    *Athlon*64*)
      cpu_arg="athlon64"
      ;;
    *Athlon*)
      cpu_arg="athlon"
      ;;
    *AMD-K7*)
      cpu_arg="athlon"
      ;;
    *Athlon*XP\ *)
      cpu_arg="athlon-xp"
      ;;
    *AMD*Sempron\(tm\)*)
      cpu_arg="athlon-mp"
      ;;
    *AMD*Athlon\(tm\)\ 64*)
      cpu_arg="k8"
      ;;
    *Opteron*)
      cpu_arg="opteron"
      ;;
    *Phenom*)
      cpu_arg="k8"
      ;;
    *AuthenticAMD*)
      cpu_arg="k6"
      ;;
    *VIA\ *)
      cpu_arg="i686"
      ;;
    # MacOSX / Intel  
    *i386*i486*)
      cpu_arg="pentium-m"
      ;;
    *i386*)
      cpu_arg="i386"
      ;;
    # Intel ia64
    *Itanium*)
      cpu_arg="itanium"
      ;;
    *IA-64*)
      cpu_arg="itanium"
      ;;
    # Solaris Sparc
    *sparc*sun4[uv]*)
      cpu_arg="sparc"
      ;;
    # Power PC
    *ppc*)
      cpu_arg="powerpc"
      ;;
    *powerpc*)
      cpu_arg="powerpc"
      ;;
    # unknown
    *)
      cpu_arg=""
      ;;
  esac

  if test "x$low_cpu_arg" = "x" ; then
    low_cpu_arg="$cpu_arg"
  fi

  if test -z "$cpu_arg" ; then
    if test "$CPUINFO" != " " ; then
      # fallback to uname if necessary
      TEST_CPUINFO=" "
      check_cpu_cflags=""
      check_cpu
      return
    fi
    echo "BUILD/check-cpu: Oops, could not find out what kind of cpu this machine is using." >&2
    check_cpu_cflags=""
    return
  fi

  if test "x$compiler" = "x" ; then
    check_compiler_cpu_flags
  fi

  if test "x$core2" = "xyes" ; then
    cpu_arg="core2"
  fi

  return 0
}

check_cpu
