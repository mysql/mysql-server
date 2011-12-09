#!/bin/sh

# Copyright (c) 2007, 2010, Oracle and/or its affiliates. All rights reserved.
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

#############################################################
# This script created by Jonas does the following	    #
# Cleans up clones and pevious builds, pulls new clones,    #
# builds, deploys, configures the tests and launches ATRT   #
#############################################################

###############
#Script setup #
##############

save_args=$*
VERSION="autotest-run.sh version 1.00"

DATE=`date '+%Y-%m-%d'`
if [ `uname -s` != "SunOS" ]
then
  if [ `uname | grep -ic cygwin || true` -ne 0 ]
  then
    HOST=`hostname`
    # Returns windows CRLF
    HOST=`echo $HOST | tr -d "\015"`
    echo "Host: '$HOST'"
  else
    HOST=`hostname -s`
  fi
else
  HOST=`hostname`
fi
export DATE HOST

set -e
ulimit -Sc unlimited

echo "`date` starting: $*"

RSYNC_RSH=ssh
export RSYNC_RSH

verbose=0
report=yes
nolock=
clonename=
RUN="daily-basic"
conf=autotest.conf
LOCK=$HOME/.autotest-lock

############################
# Read command line entries#
############################

while [ "$1" ]
do
        case "$1" in
                --verbose) verbose=`expr $verbose + 1`;;
                --conf=*) conf=`echo $1 | sed s/--conf=//`;;
                --version) echo $VERSION; exit;;
	        --suite=*) RUN=`echo $1 | sed s/--suite=//`;;
	        --run-dir=*) run_dir=`echo $1 | sed s/--run-dir=//`;;
	        --install-dir=*) install_dir0=`echo $1 | sed s/--install-dir=//`;;
	        --install-dir0=*) install_dir0=`echo $1 | sed s/--install-dir0=//`;;
	        --install-dir1=*) install_dir1=`echo $1 | sed s/--install-dir1=//`;;
	        --clone=*) clone0=`echo $1 | sed s/--clone=//`;;
	        --clone0=*) clone0=`echo $1 | sed s/--clone0=//`;;
	        --clone1=*) clone1=`echo $1 | sed s/--clone1=//`;;
	        --nolock) nolock=true;;
	        --clonename=*) clonename=`echo $1 | sed s/--clonename=//`;;
        esac
        shift
done

#################################
#Make sure the configfile exists#
#if it does not exit. if it does#
# (.) load it			# 
#################################

install_dir_save=$install_dir0
if [ -f $conf ]
then
	. $conf
else
	echo "Can't find config file: $conf"
	exit
fi
install_dir0=$install_dir_save

if [ -z "$run_dir" ]
then
    if [ "$install_dir1" ]
    then
	echo "--run-dir not specified but install_dir1 specified"
	echo "giving up"
	exit
    fi
    run_dir=$install_dir0
fi

###############################
# Validate that all interesting
#   variables where set in conf
###############################
vars="target base_dir install_dir0 hosts"
if [ "$report" ]
then
	vars="$vars result_host result_path"
fi
for i in $vars
do
  t=`echo echo \\$$i`
  if [ -z "`eval $t`" ]
  then
      echo "Invalid config: $conf, variable $i is not set"
      exit
  fi
done

###############################
#Print out the enviroment vars#
###############################

if [ $verbose -gt 0 ]
then
	env
fi

#######################################
# Check to see if the lock file exists#
# If it does exit. 		      #
#######################################

if [ -z "$nolock" ]
then
    if [ -f $LOCK ]
    then
	echo "Lock file exists: $LOCK"
	exit 1
    fi
    echo "$DATE $RUN" > $LOCK
fi

#############################
#If any errors here down, we#
# trap them, and remove the #
# Lock file before exit     #
#############################
if [ `uname -s` != "SunOS" ]
then
	trap "rm -f $LOCK" ERR
fi


###############################################
# Check that all interesting files are present#
###############################################

test_dir=$install_dir0/mysql-test/ndb
atrt=$test_dir/atrt
test_file=$test_dir/$RUN-tests.txt

if [ ! -f "$test_file" ]
then
    echo "Cant find testfile: $test_file"
    exit 1
fi

if [ ! -x "$atrt" ]
then
    echo "Cant find atrt binary at $atrt"
    exit 1
fi

############################
# check ndb_cpcc fail hosts#
############################
failed=`ndb_cpcc $hosts | awk '{ if($1=="Failed"){ print;}}'`
if [ "$failed" ]
then
  echo "Cant contact cpcd on $failed, exiting"
  exit 1
fi

#############################
# Function for replacing the#
# choose host with real host#
# names. Note $$ = PID	    #
#############################
choose(){
        SRC=$1
        TMP1=/tmp/choose.$$
        TMP2=/tmp/choose.$$.$$
        shift

        cp $SRC $TMP1
        i=1
        while [ $# -gt 0 ]
        do
                sed -e s,"CHOOSE_host$i",$1,g < $TMP1 > $TMP2
                mv $TMP2 $TMP1
                shift
                i=`expr $i + 1`
        done
        cat $TMP1
        rm -f $TMP1
}

choose_conf(){
    if [ -f $test_dir/conf-$1-$HOST.cnf ]
	then
	echo "$test_dir/conf-$1-$HOST.cnf"
    elif [ -f $test_dir/conf-$1.cnf ]
    then
	echo "$test_dir/conf-$1.cnf"
    elif [ -f $test_dir/conf-$HOST.cnf ]
    then
	echo "$test_dir/conf-$HOST.cnf"
    else
	echo "Unable to find conf file looked for" 1>&2
	echo "$test_dir/conf-$1-$HOST.cnf and" 1>&2
	echo "$test_dir/conf-$HOST.cnf" 1>&2
	echo "$test_dir/conf-$1.cnf" 1>&2
	exit
    fi
}

#########################################
# Count how many computers we have ready#
#########################################

count_hosts(){
    ch="CHOOSE_host"
    list=`grep $ch $1 | sed 's!,! !g'`
    cnt=`for i in $list; do echo $i; done | grep $ch | sort | uniq | wc -l`
    echo $cnt
}

conf=`choose_conf $RUN`
count=`count_hosts $conf`
avail=`echo $hosts | wc -w`
if  [ $count -gt $avail ]
    then
    echo "Not enough hosts"
    echo "Needs: $count available: $avail ($avail_hosts)"
    exit 1
fi

###
# Make directories needed

p=`pwd`
run_dir=$run_dir/run-$RUN-$clone0-$target
res_dir=$base_dir/result-$RUN-$clone0-$target/$DATE
tar_dir=$base_dir/saved-results

mkdir -p $run_dir $res_dir $tar_dir
rm -rf $res_dir/* $run_dir/*

###
#
# Do sed substitiutions
# 
cd $run_dir
mkdir run

if [ `uname | grep -ic cygwin || true` -ne 0 ]
then
  run_dir=`cygpath -m $run_dir`
  install_dir0=`cygpath -u $install_dir0`
  if [ "$install_dir1" ]
  then
    install_dir1=`cygpath -u $install_dir1`
  fi
  test_dir=`cygpath -m $test_dir`
fi

choose $conf $hosts > d.tmp.$$
sed -e s,CHOOSE_dir,"$run_dir/run",g < d.tmp.$$ > my.cnf

prefix="--prefix=$install_dir0"
if [ "$install_dir1" ]
then
    prefix="$prefix --prefix1=$install_dir1"
fi

# Setup configuration
$atrt Cdq $prefix my.cnf

# Start...
args=""
args="--report-file=report.txt"
args="$args --log-file=log.txt"
args="$args --testcase-file=$test_dir/$RUN-tests.txt"
args="$args $prefix"
$atrt $args my.cnf

# Make tar-ball
[ -f log.txt ] && mv log.txt $res_dir
[ -f report.txt ] && mv report.txt $res_dir
[ "`find . -name 'result*'`" ] && mv result* $res_dir
cd $res_dir

echo "date=$DATE" > info.txt
echo "suite=$RUN" >> info.txt
echo "clone=$clone0" >> info.txt
echo "arch=$target" >> info.txt
echo "host=$HOST" >> info.txt
if [ "$clone1" ]
then
    echo "clone1=$clone1" >> info.txt
fi
if [ "$clonename" ]
then
    echo "clonename=$clonename" >> info.txt
else
    echo "clonename=$clone0" >> info.txt
fi
find . | xargs chmod ugo+r

# Try to pack and transfer as much as possible
set +e

cd ..
p2=`pwd`
cd ..
tarfile=res.$RUN.$clone0.$target.$DATE.$HOST.$$.tgz
if [ `uname -s` != "SunOS" ]
then
    tar cfz $tar_dir/$tarfile `basename $p2`/$DATE
else
    tarfile2=res.$RUN.$clone0.$target.$DATE.$HOST.$$.tar
    tar cf $tar_dir/$tarfile2 `basename $p2`/$DATE
    gzip -c $tar_dir/$tarfile2 > $tar_dir/$tarfile
    rm -f $tar_dir/$tarfile2
fi

if [ "$report" ]
then
    scp $tar_dir/$tarfile $result_host:$result_path/${tarfile}.upload
    ssh $result_host mv $result_path/${tarfile}.upload $result_path/${tarfile}
fi

cd $p
rm -rf $res_dir $run_dir

if [ -z "$nolock" ]
then
    rm -f $LOCK
fi
