#!/bin/sh
# Copyright (C) 1997, 1998, 1999 TCX DataKonsult AB & Monty Program KB & Detron HB
# For a more info consult the file COPYRIGHT distributed with this file

# This scripts creates the privilege tables db, host, user, tables_priv,
# columns_priv in the mysql database, as well as the func table.
#
# All arguments (exept -IN-RPM as a first argument) to this script are
# passed to mysqld

ldata=@localstatedir@
execdir=@libexecdir@
bindir=@bindir@
sbindir=@sbindir@
force=0
IN_RPM=0
defaults=

while [ "x$1" != x ]
do
   case "$1" in
   -*) eqvalue="`echo $1 |sed 's/[-_a-zA-Z0-9]*=//'`"
       case "$1" in
       -IN-RPM) IN_RPM=1
                ;;
       --force) force=1
                ;;
       --no-defaults=*) CONFIG_FILES=/nonexistent
                ;;
       --defaults-file=*) CONFIG_FILES="$eqvalue"
                ;;
       --basedir=*) SETVARS="$SETVARS basedir=\"$eqvalue\"; bindir=\"$eqvalue/bon\"; execdir=\"$eqvalue/libexec\"; sbindir=\"$eqvalue/sbin\"; "
                ;;
       --ldata=*|--datadir=*) SETVARS="$SETVARS ldata=\"$eqvalue\";"
                ;;
       --user=*) SETVARS="$SETVARS user=\"$eqvalue\";"
                ;;
       esac
       ;;
   esac
   shift
done
             
GetCNF () {

VARIABLES="basedir bindir datadir sbindir user pid-file log port socket"
# set it not already set
CONFIG_FILES=${CONFIG_FILES:-"/etc/my.cnf ./my.cnf $HOME/.my.cnf"}

for c in $CONFIG_FILES
do
   if [ -f $c ]
   then
      #echo "Processing $c..."
      for v in $VARIABLES
      do
         # This method assumes last of duplicate $variable entries will be the
         # value set ([mysqld])
         # This could easily be rewritten to gather [xxxxx]-specific entries,
         # but for now it looks like only the mysqld ones are needed for
         # server startup scripts
         eval `sed -n -e '/^$/d' -e '/^#/d' -e 's,[ 	],,g' -e '/=/p' $c |\
         awk -F= -v v=$v '{if ($1 == v) printf ("thevar=\"%s\"\n", $2)}'`

         # it would be easier if the my.cnf and variable values were
         # all matched, but since they aren't we need to map them here.
         case $v in
         pid-file) v=pid_file ;;
              log) v=log_file ;;
          datadir) v=ldata ;;
         esac

         # As long as $thevar isn't blank, use it to set or override current
         # value
         [ "$thevar" != "" ] && eval $v=$thevar
            
      done
   #else
   #   echo "No $c config file."
   fi
done
}

# run function to get config values
GetCNF

# Override/set with command-line values
eval $SETVARS


mdata=$ldata/mysql

if test ! -x $execdir/mysqld
then
  if test "$IN_RPM" -eq 1
  then
    echo "FATAL ERROR $execdir/mysqld not found!"
    exit 1
  else
    echo "Didn't find $execdir/mysqld"
    echo "You should do a 'make install' before executing this script"
    exit 1
  fi
fi

hostname=`@HOSTNAME@`		# Install this too in the user table

# Check if hostname is valid
if test "$IN_RPM" -eq 0 -a $force -eq 0
then
  resolved=`$bindir/resolveip $hostname 2>&1`
  if [ $? -ne 0 ]
  then
    resolved=`$bindir/resolveip localhost 2>&1`
    if [ $? -eq 0 ]
    then
      echo "Sorry, the host '$hostname' could not be looked up."
      echo "Please configure the 'hostname' command to return a correct hostname."
      echo "If you want to solve this at a later stage, restart this script with"
      echo "the --force option"
      exit 1
    fi
    echo "WARNING: Your libc libraries are not 100 % compatible with this MySQL version"
    echo "mysqld should work normally with the exception that host name resolving"
    echo "will not work.  This means that you should use IP addresses instead"
    echo "of hostnames when specifying MySQL privileges !"
  fi
fi

# Create database directories mysql & test
if test "$IN_RPM" -eq 0
then
  if test ! -d $ldata; then mkdir $ldata; chmod 700 $ldata ; fi
  if test ! -d $ldata/mysql; then mkdir $ldata/mysql;  chmod 700 $ldata/mysql ; fi
  if test ! -d $ldata/test; then mkdir $ldata/test;  chmod 700 $ldata/test ; fi
  if test -w / -a ! -z "$user"; then
    chown $user $ldata $ldata/mysql $ldata/test;
  fi
fi

# Initialize variables
c_d="" i_d=""
c_h="" i_h=""
c_u="" i_u=""
c_f="" i_f=""
c_t="" c_c=""

# Check for old tables
if test ! -f $mdata/db.frm
then
  echo "Creating db table"

  # mysqld --bootstrap wants one command/line
  c_d="$c_d CREATE TABLE db ("
  c_d="$c_d   Host char(60) DEFAULT '' NOT NULL,"
  c_d="$c_d   Db char(64) DEFAULT '' NOT NULL,"
  c_d="$c_d   User char(16) DEFAULT '' NOT NULL,"
  c_d="$c_d   Select_priv enum('N','Y') DEFAULT 'N' NOT NULL,"
  c_d="$c_d   Insert_priv enum('N','Y') DEFAULT 'N' NOT NULL,"
  c_d="$c_d   Update_priv enum('N','Y') DEFAULT 'N' NOT NULL,"
  c_d="$c_d   Delete_priv enum('N','Y') DEFAULT 'N' NOT NULL,"
  c_d="$c_d   Create_priv enum('N','Y') DEFAULT 'N' NOT NULL,"
  c_d="$c_d   Drop_priv enum('N','Y') DEFAULT 'N' NOT NULL,"
  c_d="$c_d   Grant_priv enum('N','Y') DEFAULT 'N' NOT NULL,"
  c_d="$c_d   References_priv enum('N','Y') DEFAULT 'N' NOT NULL,"
  c_d="$c_d   Index_priv enum('N','Y') DEFAULT 'N' NOT NULL,"
  c_d="$c_d   Alter_priv enum('N','Y') DEFAULT 'N' NOT NULL,"
  c_d="$c_d PRIMARY KEY Host (Host,Db,User),"
  c_d="$c_d KEY User (User)"
  c_d="$c_d )"
  c_d="$c_d comment='Database privileges';"
  
  i_d="INSERT INTO db VALUES ('%','test','','Y','Y','Y','Y','Y','Y','N','Y','Y','Y');
  INSERT INTO db VALUES ('%','test\_%','','Y','Y','Y','Y','Y','Y','N','Y','Y','Y');"
fi

if test ! -f $mdata/host.frm
then
  echo "Creating host table"

  c_h="$c_h CREATE TABLE host ("
  c_h="$c_h  Host char(60) DEFAULT '' NOT NULL,"
  c_h="$c_h  Db char(64) DEFAULT '' NOT NULL,"
  c_h="$c_h  Select_priv enum('N','Y') DEFAULT 'N' NOT NULL,"
  c_h="$c_h  Insert_priv enum('N','Y') DEFAULT 'N' NOT NULL,"
  c_h="$c_h  Update_priv enum('N','Y') DEFAULT 'N' NOT NULL,"
  c_h="$c_h  Delete_priv enum('N','Y') DEFAULT 'N' NOT NULL,"
  c_h="$c_h  Create_priv enum('N','Y') DEFAULT 'N' NOT NULL,"
  c_h="$c_h  Drop_priv enum('N','Y') DEFAULT 'N' NOT NULL,"
  c_h="$c_h  Grant_priv enum('N','Y') DEFAULT 'N' NOT NULL,"
  c_h="$c_h  References_priv enum('N','Y') DEFAULT 'N' NOT NULL,"
  c_h="$c_h  Index_priv enum('N','Y') DEFAULT 'N' NOT NULL,"
  c_h="$c_h  Alter_priv enum('N','Y') DEFAULT 'N' NOT NULL,"
  c_h="$c_h  PRIMARY KEY Host (Host,Db)"
  c_h="$c_h )"
  c_h="$c_h comment='Host privileges;  Merged with database privileges';"
fi

if test ! -f $mdata/user.frm
then
  echo "Creating user table"

  c_u="$c_u CREATE TABLE user ("
  c_u="$c_u   Host char(60) DEFAULT '' NOT NULL,"
  c_u="$c_u   User char(16) DEFAULT '' NOT NULL,"
  c_u="$c_u   Password char(16) DEFAULT '' NOT NULL,"
  c_u="$c_u   Select_priv enum('N','Y') DEFAULT 'N' NOT NULL,"
  c_u="$c_u   Insert_priv enum('N','Y') DEFAULT 'N' NOT NULL,"
  c_u="$c_u   Update_priv enum('N','Y') DEFAULT 'N' NOT NULL,"
  c_u="$c_u   Delete_priv enum('N','Y') DEFAULT 'N' NOT NULL,"
  c_u="$c_u   Create_priv enum('N','Y') DEFAULT 'N' NOT NULL,"
  c_u="$c_u   Drop_priv enum('N','Y') DEFAULT 'N' NOT NULL,"
  c_u="$c_u   Reload_priv enum('N','Y') DEFAULT 'N' NOT NULL,"
  c_u="$c_u   Shutdown_priv enum('N','Y') DEFAULT 'N' NOT NULL,"
  c_u="$c_u   Process_priv enum('N','Y') DEFAULT 'N' NOT NULL,"
  c_u="$c_u   File_priv enum('N','Y') DEFAULT 'N' NOT NULL,"
  c_u="$c_u   Grant_priv enum('N','Y') DEFAULT 'N' NOT NULL,"
  c_u="$c_u   References_priv enum('N','Y') DEFAULT 'N' NOT NULL,"
  c_u="$c_u   Index_priv enum('N','Y') DEFAULT 'N' NOT NULL,"
  c_u="$c_u   Alter_priv enum('N','Y') DEFAULT 'N' NOT NULL,"
  c_u="$c_u   PRIMARY KEY Host (Host,User)"
  c_u="$c_u )"
  c_u="$c_u comment='Users and global privileges';"

  i_u="INSERT INTO user VALUES ('localhost','root','','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y');
  INSERT INTO user VALUES ('$hostname','root','','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y');
  
  REPLACE INTO user VALUES ('localhost','root','','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y');
  REPLACE INTO user VALUES ('$hostname','root','','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y');
  
  INSERT INTO user VALUES ('localhost','','','N','N','N','N','N','N','N','N','N','N','N','N','N','N');
  INSERT INTO user VALUES ('$hostname','','','N','N','N','N','N','N','N','N','N','N','N','N','N','N');"
fi

if test ! -f $mdata/func.frm
then
  echo "Creating func table"

  c_f="$c_f CREATE TABLE func ("
  c_f="$c_f   name char(64) DEFAULT '' NOT NULL,"
  c_f="$c_f   ret tinyint(1) DEFAULT '0' NOT NULL,"
  c_f="$c_f   dl char(128) DEFAULT '' NOT NULL,"
  c_f="$c_f   type enum ('function','aggregate') NOT NULL,"
  c_f="$c_f   PRIMARY KEY (name)"
  c_f="$c_f )"
  c_f="$c_f   comment='User defined functions';"
fi

if test ! -f $mdata/tables_priv.frm
then
  echo "Creating tables_priv table"

  c_t="$c_t CREATE TABLE tables_priv ("
  c_t="$c_t   Host char(60) DEFAULT '' NOT NULL,"
  c_t="$c_t   Db char(64) DEFAULT '' NOT NULL,"
  c_t="$c_t   User char(16) DEFAULT '' NOT NULL,"
  c_t="$c_t   Table_name char(60) DEFAULT '' NOT NULL,"
  c_t="$c_t   Grantor char(77) DEFAULT '' NOT NULL,"
  c_t="$c_t   Timestamp timestamp(14),"
  c_t="$c_t   Table_priv set('Select','Insert','Update','Delete','Create','Drop','Grant','References','Index','Alter') DEFAULT '' NOT NULL,"
  c_t="$c_t   Column_priv set('Select','Insert','Update','References') DEFAULT '' NOT NULL,"
  c_t="$c_t   PRIMARY KEY (Host,Db,User,Table_name),"
  c_t="$c_t   KEY Grantor (Grantor)"
  c_t="$c_t )"
  c_t="$c_t   comment='Table privileges';"
fi

if test ! -f $mdata/columns_priv.frm
then
  echo "Creating columns_priv table"

  c_c="$c_c CREATE TABLE columns_priv ("
  c_c="$c_c   Host char(60) DEFAULT '' NOT NULL,"
  c_c="$c_c   Db char(64) DEFAULT '' NOT NULL,"
  c_c="$c_c   User char(16) DEFAULT '' NOT NULL,"
  c_c="$c_c   Table_name char(64) DEFAULT '' NOT NULL,"
  c_c="$c_c   Column_name char(64) DEFAULT '' NOT NULL,"
  c_c="$c_c   Timestamp timestamp(14),"
  c_c="$c_c   Column_priv set('Select','Insert','Update','References') DEFAULT '' NOT NULL,"
  c_c="$c_c   PRIMARY KEY (Host,Db,User,Table_name,Column_name)"
  c_c="$c_c )"
  c_c="$c_c   comment='Column privileges';"
fi

 if $execdir/mysqld $defaults --bootstrap --skip-grant-tables \
    --basedir=@prefix@ --datadir=$ldata "$@" << END_OF_DATA
use mysql;
$c_d
$i_d

$c_h
$i_h

$c_u
$i_u

$c_f
$i_f

$c_t
$c_c
END_OF_DATA
then
  echo ""
  if test "$IN_RPM" -eq 0
  then
    echo "To start mysqld at boot time you have to copy support-files/mysql.server"
    echo "to the right place for your system"
    echo
  fi
  echo "PLEASE REMEMBER TO SET A PASSWORD FOR THE MySQL root USER !"
  echo "This is done with:"
  echo "$bindir/mysqladmin -u root -p password 'new-password'"
  echo "$bindir/mysqladmin -u root -h $hostname -p password 'new-password'"
  echo "See the manual for more instructions."
  #
  # Print message about upgrading unless we have created a new db table.
  if test -z "$c_d"
  then
    echo
    echo "NOTE:  If you are upgrading from a MySQL <= 3.22.10 you should run"
    echo "the $bindir/mysql_fix_privilege_tables. Otherwise you will not be"
    echo "able to use the new GRANT command!"
  fi
  echo
  if test -z "$IN_RPM"
  then
    echo "You can start the MySQL demon with:"
    echo "cd @prefix@ ; $bindir/safe_mysqld &"
    echo
    echo "You can test the MySQL demon with the benchmarks in the 'sql-bench' directory:"
    echo "cd sql-bench ; run-all-tests"
    echo
  fi
  echo "Please report any problems with the @scriptdir@/mysqlbug script!"
  echo
  echo "The latest information about MySQL is available on the web at"
  echo "http://www.mysql.com"
  echo "Support MySQL by buying support/licenses at https://order.mysql.com"
  echo 
  exit 0
else
  echo "Installation of grant tables failed!"
  echo
  echo "Examine the logs in $ldata for more information."
  echo "You can also try to start the mysqld demon with:"
  echo "$execdir/mysqld --skip-grant &"
  echo "You can use the command line tool"
  echo "$bindir/mysql to connect to the mysql"
  echo "database and look at the grant tables:"
  echo
  echo "shell> $bindir/mysql -u root mysql"
  echo "mysql> show tables"
  echo
  echo "Try 'mysqld --help' if you have problems with paths. Using --log"
  echo "gives you a log in $ldata that may be helpful."
  echo
  echo "The latest information about MySQL is available on the web at"
  echo "http://www.mysql.com"
  echo "Please consult the MySQL manual section: 'Problems running mysql_install_db',"
  echo "and the manual section that describes problems on your OS."
  echo "Another information source is the MySQL email archive."
  echo "Please check all of the above before mailing us!"
  echo "And if you do mail us, you MUST use the @scriptdir@/mysqlbug script!"
  exit 1
fi
