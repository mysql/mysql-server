#! /bin/sh

if [ -z "$NDB_TOP" ]
then
	echo "You have not set NDB_TOP. Exiting" 1>&2
	exit 1
fi

if [ -z "$NDB_SCI" ]
then
	NDB_SCI=N
fi

os=`uname -s`
case $os in
Linux)
	NDB_OS=LINUX
	NDB_ARCH=x86
	NDB_COMPILER=GCC
	;;
Darwin)
	NDB_OS=MACOSX
	NDB_ARCH=POWERPC
	NDB_COMPILER=GCC
	;;
HP-UX)
	NDB_OS=HPUX
	NDB_ARCH=HPPA
	NDB_COMPILER=GCC
	;;
CYGWIN_NT-5.0)
	NDB_OS=WIN32
	NDB_ARCH=x86
	NDB_COMPILER=VC7
	;;
*)
	if [ "$os" = "SunOS" ] && [ `uname -r` = "5.6" ]
	then
		NDB_OS=OSE
		NDB_ARCH=PPC750
		NDB_COMPILER=DIAB
	else
		NDB_OS=SOLARIS
		NDB_ARCH=SPARC
		NDB_COMPILER=GCC
	fi;;
esac

if [ -z "$NDB_ODBC" ]
then
  val=N
  if [ -f /usr/include/sqlext.h -o -f /usr/local/include/sqlext.h ]
  then
	  val=Y
  fi
  case $NDB_OS in
  LINUX)
	NDB_ODBC=$val
	;;
  MACOSX)
	NDB_ODBC=$val
	;;
  *)
        NDB_ODBC=N
	;;
  esac
fi


mch=`uname -m`
case $mch in
x86_64)
	NDB_ARCH=x86_64
	;;
*)
	;;
esac

if [ -f $NDB_TOP/config/Makefile ]
then
TERMCAP_LIB=`grep TERMCAP_LIB $NDB_TOP/config/Makefile | sed -e s,"TERMCAP_LIB.*=.*-l","",g`
fi
if [ "$TERMCAP_LIB" = "" ]
then
TERMCAP_LIB=termcap
fi

# Allow for selecting GCC, but must be 2nd parameter
if [ $# -gt 1 -a "$2" = "-GCC" ]
then
	NDB_COMPILER=GCC
fi

(
	echo "# This file was automatically generated `date`"
	echo "NDB_OS       := $NDB_OS"
	echo "NDB_ARCH     := $NDB_ARCH"
	echo "NDB_COMPILER := $NDB_COMPILER"

	if [ $# -gt 0 -a "$1" = "-R" ]
	then
		echo "NDB_VERSION  := RELEASE"
	else
		echo "NDB_VERSION  := DEBUG"
	fi

	echo "NDB_SCI      := $NDB_SCI"
	echo "NDB_ODBC     := $NDB_ODBC"
	echo "TERMCAP_LIB  := $TERMCAP_LIB"
) > $NDB_TOP/config/config.mk

exit 0

