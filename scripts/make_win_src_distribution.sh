#!/bin/sh

#
# Script to create a Windows src package
#

version=@VERSION@
export version
CP="cp -p"

DEBUG=0
SILENT=0
SUFFIX=""
DIRNAME=""
OUTTAR="0"
OUTZIP="0"

#
# An "abort" function taking a variable number of strings (one per line)
#

abort()
{
  for line
  do
    echo "$line"
  done
  exit 1
}


#
# This script must run from MySQL top directory
#

if [ ! -f scripts/make_win_src_distribution ]; then
  abort "ERROR : You must run this script from the MySQL top-level directory"
fi
SOURCE=`pwd`

#
# Check for source compilation/configuration
#

if [ ! -f sql/sql_yacc.cc ]; then
  abort "ERROR : Sorry, you must run this script after the complete build," \
        "        hope you know what you are trying to do !!"
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
  echo "  --suffix  Suffix name for the package"
  echo "  --dirname Directory name to copy files (intermediate)"
  echo "  --silent  Do not list verbosely files processed"
  echo "  --tar     Create tar.gz package"
  echo "  --zip     Create zip package"
  echo "  --help    Show this help message"

  exit 0
}

#
# Parse the input arguments
#

parse_arguments() {
  for arg do
    case "$arg" in
      --add-tar)  ADDTAR=1 ;;
      --debug)    DEBUG=1;;
      --tmp=*)    TMP=`echo "$arg" | sed -e "s;--tmp=;;"` ;;
      --suffix=*) SUFFIX=`echo "$arg" | sed -e "s;--suffix=;;"` ;;
      --dirname=*) DIRNAME=`echo "$arg" | sed -e "s;--dirname=;;"` ;;
      --silent)   SILENT=1 ;;
      --tar)      OUTTAR=1 ;;
      --zip)      OUTZIP=1 ;;
      --help)     show_usage ;;
      *)          abort "Unknown argument '$arg'"
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
# Convert argument file from unix to DOS text
#

unix_to_dos()
{
  for arg do
    print_debug "Replacing LF -> CRLF from '$arg'"

    awk '{sub(/$/,"\r");print}' < $arg   > $arg.tmp
    rm -f $arg
    mv $arg.tmp $arg
  done
}


#
# Create a tmp dest directory to copy files
#

BASE=$TMP/my_win_dist$SUFFIX

if [ -d $BASE ] ; then
  print_debug "Destination directory '$BASE' already exists, deleting it"
  rm -r -f $BASE
fi

$CP -r $SOURCE/VC++Files $BASE
# This includes an implicit 'mkdir $BASE' !

#
# Process version tags in InstallShield files
#

vreplace()
{
  for arg do
    unix_to_dos $arg
    cat $arg | sed -e 's!@''VERSION''@!@VERSION@!' > $arg.tmp
    rm -f $arg
    mv $arg.tmp $arg
  done
}

if test -d $BASE/InstallShield
then
  for d in 4.1.XX-gpl 4.1.XX-pro 4.1.XX-classic
  do
    cd $BASE/InstallShield/$d/String\ Tables/0009-English
    vreplace value.shl
    cd ../../Setup\ Files/Compressed\ Files/Language\ Independent/OS\ Independent
    vreplace infolist.txt
  done
fi

#
# Move all error message files to root directory
#

$CP -r $SOURCE/sql/share $BASE/
rm -r -f "$BASE/share/Makefile"
rm -r -f "$BASE/share/Makefile.in"
rm -r -f "$BASE/share/Makefile.am"

mkdir $BASE/Docs $BASE/extra $BASE/include

#
# Copy directory files
#

copy_dir_files()
{
  for arg do
    print_debug "Copying files from directory '$arg'"
    cd $SOURCE/$arg
    if [ ! -d $BASE/$arg ]; then
       print_debug "Creating directory '$arg'"
       mkdir $BASE/$arg
     fi
    for i in *.c *.cpp *.h *.ih *.i *.ic *.asm *.def *.hpp *.dsp *.dsw \
             README INSTALL* LICENSE AUTHORS NEWS ChangeLog \
             *.inc *.test *.result *.pem Moscow_leap des_key_file \
             *.vcproj *.sln *.dat *.000001 *.require *.opt
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

    cd $SOURCE
    (
    find $arg -type d \
              -and -not -path \*SCCS\* \
              -and -not -path \*.deps\* \
              -and -not -path \*.libs\* \
              -and -not -path \*autom4te.cache -print
    )|(
      while read v
      do
        copy_dir_files $v
      done
    )

  done
}

#
# Input directories to be copied
#

for i in client dbug extra heap include \
         libmysql libmysqld myisam \
         myisammrg mysys regex sql strings sql-common sql/examples \
         tools vio zlib
do
  copy_dir_files $i
done

#
# Create project files for ndb
#
make -C $SOURCE/ndb windoze

#
# Input directories to be copied recursively
#

for i in bdb innobase ndb extra/yassl server-tools
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
for i in COPYING ChangeLog README EXCEPTIONS-CLIENT\
         INSTALL-SOURCE INSTALL-WIN \
         INSTALL-WIN-SOURCE \
         Docs/INSTALL-BINARY Docs/manual.chm
do
  print_debug "Copying file '$i'"
  if [ -f $i ]
  then
    $CP $i $BASE/$i
  fi
done

cp extra/sql_state.h extra/mysqld_error.h $BASE/include

#
# support files
#
mkdir $BASE/support-files

# Rename the cnf files to <file>.ini
for i in support-files/*.cnf
do
  i=`echo $i | sed 's/.cnf$//g'`
  cp $i.cnf $BASE/$i.ini
done

#
# Raw dirs from source tree
#

for i in scripts sql-bench mysql-test SSL tests
do
  print_debug "Copying directory '$i'"
  if [ -d $i ]
  then
    if [ -d $BASE/$i ]
    then
      $CP -R $i $BASE
    else
      $CP -R $i $BASE/$i
    fi
  fi
  # But remove object files from destination
  find $BASE/$i -type f -name \*.o | xargs rm -f
done

#
# Fix some windows files to avoid compiler warnings
#

./extra/replace std:: "" < $BASE/sql/sql_yacc.cpp | sed '/^ *switch (yytype)$/ { N; /\n *{$/ { N; /\n *default:$/ { N; /\n *break;$/ { N; /\n *}$/ d; };};};} ' > $BASE/sql/sql_yacc.cpp-new
mv $BASE/sql/sql_yacc.cpp-new $BASE/sql/sql_yacc.cpp

#
# Search the tree for plain text files and adapt the line end marker
#
find $BASE \( -name "*.dsp" -o -name "*.dsw" -o -name "*.cnf" -o -name "*.ini" \
           -o -name COPYING -o -name ChangeLog -o -name EXCEPTIONS-CLIENT \
           -o -name "INSTALL*" -o -name LICENSE -o -name "README*" \
           -o -name "*.vcproj" -o -name "*.sln" \) -type f -print \
| while read v
  do
    unix_to_dos $v
  done
# File extension '.txt' matches too many other files, error messages etc.
unix_to_dos $BASE/Docs/*.txt 

mv $BASE/README $BASE/README.txt

#
# Clean up if we did this from a bk tree
#

if [ -d $BASE/SSL/SCCS ]
then
  find $BASE/ -type d -name SCCS -printf " \"%p\"" | xargs rm -r -f
fi
find $BASE/ -type d -name .deps -printf " \"%p\"" | xargs rm -r -f
find $BASE/ -type d -name .libs -printf " \"%p\"" | xargs rm -r -f
rm -r -f "$BASE/mysql-test/var"

#
# Initialize the initial data directory
#

if [ -f scripts/mysql_install_db ]; then
  print_debug "Initializing the 'data' directory"
  scripts/mysql_install_db --no-defaults --windows --datadir=$BASE/data
  if test "$?" = 1
  then
    exit 1;
  fi
fi

#
# Specify the distribution package name and copy it
#

if test -z $DIRNAME
then
  NEW_DIR_NAME=mysql@MYSQL_SERVER_SUFFIX@-$version$SUFFIX
else
  NEW_DIR_NAME=$DIRNAME
fi
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

if [ "$OUTTAR" = "0" ]; then
  if [ "$OUTZIP" = "0" ]; then
    OUTZIP=1
  fi
fi

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
      OPT="-r"
      EXT=".zip"
      NEED_COMPRESS=0
      if [ "$SILENT" = "1" ] ; then
        OPT="$OPT -q"
      fi
    fi
  done
}


#
# Create the archive
#
create_archive()
{

  print_debug "Using $tar to create archive"

  cd $TMP

  rm -f $SOURCE/$NEW_NAME$EXT
  $tar $OPT $SOURCE/$NEW_NAME$EXT $NEW_DIR_NAME
  cd $SOURCE

  if [ "$NEED_COMPRESS" = "1" ]
  then
    print_debug "Compressing archive"
    gzip -9 $NEW_NAME$EXT
    EXT="$EXT.gz"
  fi

  if [ "$SILENT" = "0" ] ; then
    echo "$NEW_NAME$EXT created successfully !!"
  fi
}

if [ "$OUTTAR" = "1" ]; then
  set_tarzip_options 'tar'

  tar=`which_1 $ZIPFILE1 $ZIPFILE2`
  if test "$?" = "1" -o "$tar" = ""
  then
    print_debug "Search failed for '$ZIPFILE1', '$ZIPFILE2', using default 'tar'"
    tar=tar
    set_tarzip_options 'tar'
  fi
  
  create_archive 
fi

if [ "$OUTZIP" = "1" ]; then
  set_tarzip_options 'zip'

  tar=`which_1 $ZIPFILE1 $ZIPFILE2`
  if test "$?" = "1" -o "$tar" = ""
  then
    echo "Search failed for '$ZIPFILE1', '$ZIPFILE2', cannot create zip!"
  fi
 
  create_archive
fi

print_debug "Removing temporary directory"
rm -r -f $BASE

# End of script
