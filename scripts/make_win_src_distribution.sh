#!/bin/sh

#
# Script to create a Windows src package
#

version=@VERSION@
export version
SOURCE=`pwd`
CP="cp -p"

DEBUG=0
SILENT=0
TMP=/tmp
SUFFIX=""
OUTTAR=0

SRCDIR=$SOURCE
DSTDIR=$TMP

#
# This script must run from MySQL top directory
#

if [ ! -f scripts/make_win_src_distribution.sh ]; then
  echo "ERROR : You must run this script from the MySQL top-level directory"
  exit 1
fi
    
#
# Usage of the script
#

show_usage() {
  
  echo "MySQL utility script to create a Windows src package, and it takes"
  echo "the following arguments:"
  echo ""
  echo "  --debug   Debug, without creating the package"
  echo "  --tmp     Specify the temporary location"
  echo "  --silent  Do not list verbosely files processed"
  echo "  --tar     Create a tar.gz package instead of .zip"
  echo "  --help    Show this help message"

  exit 1
}

#
# Parse the input arguments
#

parse_arguments() {
  for arg do
    case "$arg" in
      --debug)    DEBUG=1;;
      --tmp=*)    TMP=`echo "$arg" | sed -e "s;--tmp=;;"` ;;
      --suffix=*) SUFFIX=`echo "$arg" | sed -e "s;--suffix=;;"` ;;
      --silent)   SILENT=1 ;;
      --tar)      OUTTAR=1 ;;
      --help)     show_usage ;;
      *)
  echo "Unknown argument '$arg'"
  exit 1
      ;;
    esac
  done
}

parse_arguments "$@"

#
# Currently return an error if --tar is not used
#

if [ x$OUTTAR = x0 ] &&  [ x$DEBUG = x0 ] 
then
  echo "ERROR: The default .zip doesn't yet work on Windows, as it can't load"
  echo "       the workspace files due to LF->CR+LF issues, instead use --tar"
  echo "       to create a .tar.gz package                                   "
  echo ""
  show_usage;
  exit 1
fi

#
# Create a tmp dest directory to copy files
#

BASE=$TMP/my_win_dist$SUFFIX

if [ -d $BASE ] ; then
  rm -r -f $BASE
fi

$CP -r $SOURCE/VC++Files $BASE

(
find $BASE -name *.dsw -and -not -path \*SCCS\* -print
find $BASE -name *.dsp -and -not -path \*SCCS\* -print
)|(
while read v 
do
    sed 's/$'"/`echo -e \\\r`/" $v > $v.tmp
    rm $v
    mv $v.tmp $v
done
)

# move all error message files to root directory
$CP -r $SOURCE/sql/share $BASE/

#
# Clean up if we did this from a bk tree
#

if [ -d $BASE/SCCS ]  
then
  find $BASE/ -name SCCS -print | xargs rm -r -f
  rm -rf "$BASE/InstallShield/Script Files/SCCS"  
fi
  

mkdir $BASE/Docs $BASE/extra $BASE/include 


#
# Copy directory files
#

copy_dir_files() {
  
  for arg do
    
    cd $SOURCE/$arg/
    (
      ls -A1|grep \\.[ch]$
      ls -A1|grep \\.ih$
      ls -A1|grep \\.i$
      ls -A1|grep \\.ic$
      ls -A1|grep \\.asm$ 
      ls -A1|grep README
      ls -A1|grep INSTALL
      ls -A1|grep LICENSE
    )|(
      while read v 
      do
	      $CP $SOURCE/$arg/$v $BASE/$arg/$v
      done
    )

    cd $SOURCE/$arg/
    (
      ls -A1|grep \\.cc$|sed 's/.cc$//g')|(
      while read v 
      do
  	    $CP $SOURCE/$arg/$v.cc $BASE/$arg/$v.cpp
      done
    )
  done
}

#
# Copy directory contents recursively
#

copy_dir_dirs() {

  for arg do

    basedir=$arg
      
    if [ !  -d $BASE/$arg ]; then
      mkdir $BASE/$arg
    fi
    
    copy_dir_files $arg
    
    cd $SOURCE/$arg/
    (
      ls -l |grep "^d"|awk '{print($9)}' -
    )|(
      while read dir_name
      do
        if [ x$dir_name != x"SCCS" ]
        then
          if [ ! -d $BASE/$basedir/$dir_name ]; then
            mkdir $BASE/$basedir/$dir_name
          fi
          copy_dir_files $basedir/$dir_name
        fi
      done
    )
  done
}

#
# Input directories to be copied
#

copy_dir_files 'bdb'
copy_dir_files 'bdb/build_win32'
copy_dir_files 'client'
copy_dir_files 'dbug'
copy_dir_files 'extra'
copy_dir_files 'heap'
copy_dir_files 'include'
copy_dir_files 'innobase'
copy_dir_files 'isam'
copy_dir_files 'libmysql'
copy_dir_files 'libmysqld'
copy_dir_files 'merge'
copy_dir_files 'myisam'
copy_dir_files 'myisammrg'
copy_dir_files 'mysys'
copy_dir_files 'regex'
copy_dir_files 'sql'
copy_dir_files 'strings'
copy_dir_files 'vio'
copy_dir_files 'zlib'

#
# Input directories to be copied recursively
#

copy_dir_dirs 'bdb'
copy_dir_dirs 'innobase'

# create dummy innobase configure header
if [ -f $BASE/innobase/ib_config.h ]
then
  rm -f $BASE/innobase/ib_config.h
  touch $BASE/innobase/ib_config.h
fi

#
# Copy miscellaneous files
#

$CP $SOURCE/myisam/myisampack.c $BASE/myisampack/
$CP $SOURCE/client/mysqlbinlog.cc $BASE/mysqlbinlog/mysqlbinlog.cpp
$CP $SOURCE/isam/pack_isam.c $BASE/pack_isam/pack_isam.c

cd $SOURCE
for i in COPYING ChangeLog README \
         INSTALL-SOURCE INSTALL-WIN \
         INSTALL-SOURCE-WIN \
         Docs/manual_toc.html  Docs/manual.html \
         Docs/mysqld_error.txt Docs/INSTALL-BINARY 
         
do
  if [ -f $i ] 
  then
     $CP $i $BASE/$i
  fi
done

#
# TODO: Initialize the initial data directory
#


#
# Specify the distribution package name and copy it
#

NEW_NAME=mysql@MYSQL_SERVER_SUFFIX@-$version$SUFFIX-win-src
BASE2=$TMP/$NEW_NAME
rm -r -f $BASE2
mv $BASE $BASE2
BASE=$BASE2

#
# If debugging, don't create a zip/tar/gz
#

if [ x$DEBUG = x1 ] ; then
  echo "Please check the distribution files from $BASE"
  echo "Exiting (without creating the package).."
  exit
fi

#
# This is needed to prefere gnu tar instead of tar because tar can't
# always handle long filenames
#

PATH_DIRS=`echo $PATH | sed -e 's/^:/. /' -e 's/:$/ ./' -e 's/::/ . /g' -e 's/:/ /g' `
which_1 ()
{
  for cmd
  do
    for d in $PATH_DIRS
    do
      for file in $d/$cmd
      do
	if test -x $file -a ! -d $file
	then
	  echo $file
	  exit 0
	fi
      done
    done
  done
  exit 1
}

#
# Create the result zip file
#

if [ x$OUTTAR = x1 ]; then
  ZIPFILE1=gnutar
  ZIPFILE2=gtar
  OPT=cvf
  EXT=".tar"
  NEED_COMPRESS=1
  if [ x$SILENT = x1 ] ; then
    OPT=cf
  fi
else
  ZIPFILE1=zip
  ZIPFILE2=""
  OPT="-lvr"
  EXT=".zip"
  NEED_COMPRESS=0
  if [ x$SILENT = x1 ] ; then
    OPT="-lr"
  fi
fi

tar=`which_1 $ZIPFILE1 $ZIPFILE2`
if test "$?" = "1" -o "$tar" = ""
then
  tar=tar
fi

echo "Using $tar to create archive"
cd $TMP

$tar $OPT $SOURCE/$NEW_NAME$EXT $NEW_NAME
cd $SOURCE

if [ x$NEED_COMPRESS = x1 ]
then
  echo "Compressing archive"
  gzip -9 $NEW_NAME$EXT
  EXT=".tar.gz"
fi

echo "Removing temporary directory"
rm -r -f $BASE

echo "$NEW_NAME$EXT created successfully !!"

#
# End of script
#



