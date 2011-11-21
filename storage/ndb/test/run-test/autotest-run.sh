#!/bin/sh

# Copyright (c) 2007 MySQL AB
# Use is subject to license terms.
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
HOST=`hostname -s`
export DATE HOST

set -e
ulimit -Sc unlimited

echo "`date` starting: $*"

RSYNC_RSH=ssh
export RSYNC_RSH

verbose=0
report=yes
nolock=
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
	        --install-dir=*) install_dir=`echo $1 | sed s/--install-dir=//`;;
                --clone=*) clone=`echo $1 | sed s/--clone=//`;;
	        --nolock) nolock=true;;
        esac
        shift
done

#################################
#Make sure the configfile exists#
#if it does not exit. if it does#
# (.) load it			# 
#################################

install_dir_save=$install_dir
if [ -f $conf ]
then
	. $conf
else
	echo "Can't find config file: $conf"
	exit
fi
install_dir=$install_dir_save

###############################
# Validate that all interesting
#   variables where set in conf
###############################
vars="target base_dir install_dir hosts"
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

test_dir=$install_dir/mysql-test/ndb
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
    cnt=`grep "CHOOSE_host" $1 | awk '{for(i=1; i<=NF;i++) \
    if(index($i, "CHOOSE_host") > 0) print $i;}' | sort | uniq | wc -l`
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
run_dir=$install_dir/run-$RUN-$clone-$target
res_dir=$base_dir/result-$RUN-$clone-$target/$DATE
tar_dir=$base_dir/saved-results

mkdir -p $run_dir $res_dir $tar_dir
rm -rf $res_dir/* $run_dir/*


###
#
# Do sed substitiutions
# 
cd $run_dir
choose $conf $hosts > d.tmp.$$
sed -e s,CHOOSE_dir,"$run_dir/run",g < d.tmp.$$ > my.cnf

# Setup configuration
$atrt Cdq my.cnf

# Start...
$atrt --report-file=report.txt --log-file=log.txt --testcase-file=$test_dir/$RUN-tests.txt my.cnf

# Make tar-ball
[ -f log.txt ] && mv log.txt $res_dir
[ -f report.txt ] && mv report.txt $res_dir
[ "`find . -name 'result*'`" ] && mv result* $res_dir
cd $res_dir

echo "date=$DATE" > info.txt
echo "suite=$RUN" >> info.txt
echo "clone=$clone" >> info.txt
echo "arch=$target" >> info.txt
find . | xargs chmod ugo+r

cd ..
p2=`pwd`
cd ..
tarfile=res.$RUN.$clone.$target.$DATE.$HOST.$$.tgz
tar cfz $tar_dir/$tarfile `basename $p2`/$DATE

if [ "$report" ]
then
    scp $tar_dir/$tarfile $result_host:$result_path/
fi

cd $p
rm -rf $res_dir $run_dir

if [ -z "$nolock" ]
then
    rm -f $LOCK
fi
