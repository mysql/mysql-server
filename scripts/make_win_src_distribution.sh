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
SUFFIX=""
OUTTAR=0

#
# This script must run from MySQL top directory
#

if [ ! -f scripts/make_win_src_distribution ]; then
  echo "ERROR : You must run this script from the MySQL top-level directory"
  exit 1
fi

#
# Check for source compilation/configuration
#

if [ ! -f sql/sql_yacc.cc ]; then
  echo "ERROR : Sorry, you must run this script after the complete build,"
  echo "        hope you know what you are trying to do !!"
  exit 1
fi

#
# Debug print of the status
#

print_debug() 
{
  for statement 
  do
    if [ "$DEBUG" = "1" ] ; then
      echo $statement
    fi
  done
}

#
# Usage of the script
#

show_usage() 
{
  echo "MySQL utility script to create a Windows src package, and it takes"
  echo "the following arguments:"
  echo ""
  echo "  --debug   Debug, without creating the package"
  echo "  --tmp     Specify the temporary location"
  echo "  --silent  Do not list verbosely files processed"
  echo "  --tar     Create tar.gz package instead of .zip"
  echo "  --help    Show this help message"

  exit 0
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
# Assign the tmp directory if it was set from the environment variables
#

for i in $TMP $TMPDIR $TEMPDIR $TEMP /tmp
do
  if [ "$i" ]; then 
    print_debug "Setting TMP to '$i'"
    TMP=$i 
    break
  fi
done

    
#

#
# Create a tmp dest directory to copy files
#

BASE=$TMP/my_win_dist$SUFFIX

if [ -d $BASE ] ; then
  print_debug "Destination directory '$BASE' already exists, deleting it"
  rm -r -f $BASE
fi

$CP -r $SOURCE/VC++Files $BASE
(
find $BASE \( -name "*.dsp" -o -name "*.dsw" \) -and -not -path \*SCCS\* -print
)|(
  while read v 
  do
    print_debug "Replacing LF -> CRLF from '$v'"

    # ^M -> type CTRL V + CTRL M 
    cat $v | sed 's///' | sed 's/$//' > $v.tmp
    rm $v
    mv $v.tmp $v
  done
)

#
# Move all error message files to root directory
#

$CP -r $SOURCE/sql/share $BASE/
rm -r -f "$BASE/share/Makefile"
rm -r -f "$BASE/share/Makefile.in"
rm -r -f "$BASE/share/Makefile.am"

#
# Clean up if we did this from a bk tree
#

if [ -d $BASE/SCCS ]  
then
  find $BASE/ -name SCCS -print | xargs rm -r -f
  rm -r -f "$BASE/InstallShield/Script Files/SCCS"  
fi

mkdir $BASE/Docs $BASE/extra $BASE/include


#
# Copy directory files
#

copy_dir_files() {
  
  for arg do
    print_debug "Copying files from directory '$arg'"
    cd $SOURCE/$arg/
    for i in *.c *.h *.ih *.i *.ic *.asm \
             README INSTALL* LICENSE
    do 
      if [ -f $i ] 
      then
        $CP $SOURCE/$arg/$i $BASE/$arg/$i
      fi
    done
    for i in *.cc
    do
      if [ -f $i ]
      then
        i=`echo $i | sed 's/.cc$//g'`
        $CP $SOURCE/$arg/$i.cc $BASE/$arg/$i.cpp
      fi
    done
  done
}

#
# Copy directory contents recursively
#

copy_dir_dirs() {

  for arg do

    basedir=$arg
      
    if [ ! -d $BASE/$arg ]; then
      mkdir $BASE/$arg
    fi
    
    copy_dir_files $arg
    
    cd $SOURCE/$arg/    
    for i in *
    do
      if [ -d $SOURCE/$basedir/$i ] && [ "$i" != "SCCS" ]; then
        if [ ! -d $BASE/$basedir/$i ]; then
          mkdir $BASE/$basedir/$i
        fi
        copy_dir_files $basedir/$i
      fi
    done
  done
}

#
# Input directories to be copied
#

for i in client dbug extra heap include isam \
         libmysql libmysqld merge myisam \
         myisammrg mysys regex sql strings \
         vio zlib
do
  copy_dir_files $i
done

#
# Input directories to be copied recursively
#

for i in bdb innobase
do
  copy_dir_dirs $i
done

#
# Create dummy innobase configure header
#

if [ -f $BASE/innobase/ib_config.h ]; then
  rm -f $BASE/innobase/ib_config.h
fi
touch $BASE/innobase/ib_config.h


#
# Copy miscellaneous files
#

cd $SOURCE
for i in COPYING ChangeLog README \
         INSTALL-SOURCE INSTALL-WIN \
         INSTALL-SOURCE-WIN \
         Docs/manual_toc.html  Docs/manual.html \
         Docs/mysqld_error.txt Docs/INSTALL-BINARY 
         
do
  print_debug "Copying file '$i'"
  if [ -f $i ] 
  then
    $CP $i $BASE/$i
  fi
done

#
# Initialize the initial data directory
#

if [ -f scripts/mysql_install_db ]; then 
  print_debug "Initializing the 'data' directory"
  scripts/mysql_install_db --windows --datadir=$BASE/data
fi


#
# Specify the distribution package name and copy it
#

NEW_DIR_NAME=mysql@MYSQL_SERVER_SUFFIX@-$version$SUFFIX
NEW_NAME=$NEW_DIR_NAME-win-src

BASE2=$TMP/$NEW_DIR_NAME
rm -r -f $BASE2
mv $BASE $BASE2
BASE=$BASE2

#
# If debugging, don't create a zip/tar/gz
#

if [ "$DEBUG" = "1" ] ; then
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
# Create the result zip/tar file
#

set_tarzip_options() 
{
  for arg 
  do
    if [ "$arg" = "tar" ]; then
      ZIPFILE1=gnutar
      ZIPFILE2=gtar
      OPT=cvf
      EXT=".tar"
      NEED_COMPRESS=1
      if [ "$SILENT" = "1" ] ; then
        OPT=cf
      fi
    else
      ZIPFILE1=zip
      ZIPFILE2=""
      OPT="-vr"
      EXT=".zip"
      NEED_COMPRESS=0
      if [ "$SILENT" = "1" ] ; then
        OPT="-r"
      fi
    fi
  done
}

if [ "$OUTTAR" = "1" ]; then
  set_tarzip_options 'tar'
else
  set_tarzip_options 'zip'
fi

tar=`which_1 $ZIPFILE1 $ZIPFILE2`
if test "$?" = "1" -o "$tar" = ""
then
  print_debug "Search failed for '$ZIPFILE1', '$ZIPFILE2', using default 'tar'"
  tar=tar
  set_tarzip_options 'tar'
fi

#
# Create the archive
#

print_debug "Using $tar to create archive"

cd $TMP

$tar $OPT $SOURCE/$NEW_NAME$EXT $NEW_DIR_NAME
cd $SOURCE

if [ "$NEED_COMPRESS" = "1" ]
then
  print_debug "Compressing archive"
  gzip -9 $NEW_NAME$EXT
  EXT="$EXT.gz"
fi

print_debug "Removing temporary directory"
rm -r -f $BASE

echo "$NEW_NAME$EXT created successfully !!"

# End of script



