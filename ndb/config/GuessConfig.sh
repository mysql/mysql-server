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
  NDB_ODBC=N
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

# defaults
NDB_VERSION=DEBUG
PACKAGE=
VERSION=

parse_arguments() {
  for arg do
    case "$arg" in
      -GCC)       NDB_COMPILER=GCC ;;
      -R)         NDB_VERSION=RELEASE ;;
      -D)         NDB_VERSION=DEBUG ;;
      --PACKAGE=*) PACKAGE=`echo "$arg" | sed -e "s;--PACKAGE=;;"` ;;
      --VERSION=*) VERSION=`echo "$arg" | sed -e "s;--VERSION=;;"` ;;
      *)
        echo "Unknown argument '$arg'"
        exit 1
        ;;
    esac
  done
}

parse_arguments "$@"

(
	echo "# This file was automatically generated `date`"
	echo "NDB_OS       := $NDB_OS"
	echo "NDB_ARCH     := $NDB_ARCH"
	echo "NDB_COMPILER := $NDB_COMPILER"
	echo "NDB_VERSION  := $NDB_VERSION"
	echo "NDB_SCI      := $NDB_SCI"
	echo "NDB_ODBC     := $NDB_ODBC"
	echo "TERMCAP_LIB  := $TERMCAP_LIB"
	echo "PACKAGE      := $PACKAGE"
	echo "VERSION      := $VERSION"
) > $NDB_TOP/config/config.mk

exit 0

