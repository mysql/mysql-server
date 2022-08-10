#!/bin/sh

# Copyright (c) 2005, 2022, Oracle and/or its affiliates.
# Use is subject to license terms
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2.0,
# as published by the Free Software Foundation.
#
# This program is also distributed with certain software (including
# but not limited to OpenSSL) that is licensed under separate terms,
# as designated in a particular file or component or in included license
# documentation.  The authors of MySQL hereby grant you an additional
# permission to link the program and your derivative works with the
# separately licensed software that they have included with MySQL.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License, version 2.0, for more details.
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
VERSION="ndb-autotest.sh version 1.04"

DATE=`date '+%Y-%m-%d'`
HOST=`hostname -s`
export DATE HOST

set -e
ulimit -Sc unlimited

echo "`date` starting: $*"

RSYNC_RSH=ssh
export RSYNC_RSH

verbose=0
do_clone=yes
build=yes
deploy=yes
run_test=yes
config=yes
report=yes

clone=5.0-ndb
RUN="daily-basic daily-devel"
conf=autotest.conf
LOCK=$HOME/.autotest-lock

############################
# Read command line entries#
############################

while [ "$1" ]
do
        case "$1" in
                --no-clone) do_clone="";;
                --no-build) build="";;
                --no-deploy) deploy="";;
                --no-test) run_test="";;
                --no-config) config="";;
                --no-report) report="";;
                --verbose) verbose=`expr $verbose + 1`;;
                --clone=*) clone=`echo $1 | sed s/--clone=//`;;
                --conf=*) conf=`echo $1 | sed s/--conf=//`;;
                --version) echo $VERSION; exit;;
                *) RUN=$*;;
        esac
        shift
done

#################################
#Make sure the configfile exists#
#if it does not exit. if it does#
# (.) load it			# 
#################################

if [ -f $conf ]
then
	. $conf
else
	echo "Can't find config file: $conf"
	exit
fi

###############################
# Validate that all interesting
#   variables where set in conf
###############################
vars="target base_dir src_clone_base install_dir build_dir hosts configure"
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

####################################
# Setup the lock file name and path#
# Setup the clone source location  #
####################################

src_clone=$src_clone_base-$clone

#######################################
# Check to see if the lock file exists#
# If it does exit. 		      #
#######################################

if [ -f $LOCK ]
then
	echo "Lock file exists: $LOCK"
	exit 1
fi

#######################################
# If the lock file does not exist then#
# create it with date and run info    #
#######################################

echo "$DATE $RUN" > $LOCK

#############################
#If any errors here down, we#
# trap them, and remove the #
# Lock file before exit     #
#############################
if [ `uname -s` != "SunOS" ]
then
	trap "rm -f $LOCK" ERR
fi

# You can add more to this path#
################################

dst_place=${build_dir}/clone-mysql-$clone-$DATE

#########################################
# Delete source and pull down the latest#
#########################################

if [ "$do_clone" ]
then
	rm -rf $dst_place
	bk clone  $src_clone $dst_place
fi

##########################################
# Build the source, make installs, and   #
# create the database to be rsynced	 #
##########################################

if [ "$build" ]
then
	cd $dst_place
        rm -rf $install_dir/*
	if [ -x BUILD/autorun.sh ]
	then
	    ./BUILD/autorun.sh
	else
	    aclocal; autoheader; autoconf; automake
	    if [ -d storage ]
	    then
		(cd storage/innobase; aclocal; autoheader; autoconf; automake)
	    else
		(cd innobase; aclocal; autoheader; autoconf; automake)
	    fi
	fi
	eval $configure --prefix=$install_dir
	make
	make install
	(cd $install_dir; ./bin/mysql_install_db) # This will be rsynced to all
fi

################################
# check script version. If the #
# version is old, replace it   #
# and restart		       #
################################

script=$install_dir/mysql-test/ndb/ndb-autotest.sh
if [ -x $script ]
then
	$script --version > /tmp/version.$$
else
	echo $VERSION > /tmp/version.$$
fi	
match=`grep -c "$VERSION" /tmp/version.$$ | xargs echo`
rm -f /tmp/version.$$
if [ $match -eq 0 ]
then
	echo "Incorrect script version...restarting"
	cp $install_dir/mysql-test/ndb/ndb-autotest.sh /tmp/at.$$.sh
	rm -rf $install_dir $dst_place
	sh /tmp/at.$$.sh $save_args
	exit
fi

###############################################
# Check that all interesting files are present#
###############################################

test_dir=$install_dir/mysql-test/ndb
atrt=$test_dir/atrt
html=$test_dir/make-html-reports.sh
mkconfig=$install_dir/mysql-test/ndb/make-config.sh

##########################
#Setup bin and test paths#
##########################

PATH=$install_dir/bin:$test_dir:$PATH
export PATH

###########################
# This will filter out all#
# the host that did not   #
# respond. Called below   #
###########################

filter(){
	neg=$1
	shift
	while [ $# -gt 0 ]
	do
		if [ `grep -c $1 $neg | xargs echo` -eq 0 ] ; then echo $1; fi
		shift
	done
}

############################
# check ndb_cpcc fail hosts#
############################
ndb_cpcc $hosts | awk '{ if($1=="Failed"){ print;}}' > /tmp/failed.$DATE
filter /tmp/failed.$DATE $hosts > /tmp/hosts.$DATE
hosts=`cat /tmp/hosts.$DATE` 

#############################
# Push bin and test to hosts#
#############################

if [ "$deploy" ]
then
    for i in $hosts
    do
      rsync -a --delete --force --ignore-errors $install_dir/ $i:$install_dir
      ok=$?
      if [ $ok -ne 0 ]
	  then
	  echo "$i failed during rsync, excluding"
	  echo $i >> /tmp/failed.$DATE
      fi
    done
fi

###
# handle scp failed hosts
#
filter /tmp/failed.$DATE $hosts > /tmp/hosts.$DATE
hosts=`cat /tmp/hosts.$DATE` 
cat /tmp/failed.$DATE > /tmp/filter_hosts.$$

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
    if [ -f $test_dir/conf-$1-$HOST.txt ]
	then
	echo "$test_dir/conf-$1-$HOST.txt"
    elif [ -f $test_dir/conf-$1.txt ]
    then
	echo "$test_dir/conf-$1.txt"
    elif [ -f $test_dir/conf-$HOST.txt ]
	echo "$test_dir/conf-$HOST.txt"
    else
	echo "Unable to find conf file looked for" 1>&2
	echo "$test_dir/conf-$1-$HOST.txt and" 1>&2
	echo "$test_dir/conf-$HOST.txt" 1>&2
	echo "$test_dir/conf-$1.txt" 1>&2
	exit
    fi
}
######################################
# Starts ATRT and gives it the right #
# command line options. after it     #
# Gathers results and moves them     #
######################################
start(){
	rm -rf report.txt result* log.txt
	$atrt -v -v -r -R --log-file=log.txt --testcase-file=$test_dir/$2-tests.txt &
	pid=$!
	echo $pid > run.pid
	wait $pid
	rm run.pid
	[ -f log.txt ] && mv log.txt $3
	[ -f report.txt ] && mv report.txt $3
	[ "`find . -name 'result*'`" ] && mv result* $3
	cd $3
	sh $html . $1 $DATE
	cd ..
	p2=`pwd`
	cd ..
	if [ "$report" ]
	then
		tar cfz /tmp/res.$2.$$.tgz `basename $p2`/$DATE
		scp /tmp/res.$2.$$.tgz \
		    $result_host:$result_path/res.$DATE.$HOST.$2.$$.tgz
		if [ $? -eq 0 ]
		then
		    rm -f /tmp/res.$2.$$.tgz
		fi
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
#######################################################
# Calls: Choose                                       #
#	 Choose_host                                  #
#        Count_host                                   #
#	 start                                        #
# for each directory in the $RUN variable	      #
#######################################################

p=`pwd`
for dir in $RUN
do
	echo "Fixing hosts for $dir"

	run_dir=$base_dir/run-$dir-mysql-$clone-$target
	res_dir=$base_dir/result-$dir-mysql-$clone-$target/$DATE

	mkdir -p $run_dir $res_dir
	rm -rf $res_dir/*
	cd $run_dir

	if [ "$config" ]
	then
	    rm -rf $run_dir/*

	    conf=`choose_conf $dir`
	    count=`count_hosts $conf`
	    avail_hosts=`filter /tmp/filter_hosts.$$ $hosts`
	    avail=`echo $avail_hosts | wc -w`
	    if  [ $count -gt $avail ]
	    then
		echo "Not enough hosts"
		echo "Needs: $count available: $avail ($avail_hosts)"
		break;
	    fi

	    run_hosts=`echo $avail_hosts| \
                       awk '{for(i=1;i<='$count';i++)print $i;}'`
	    echo $run_hosts >> /tmp/filter_hosts.$$	
	
	    choose $conf $run_hosts > d.tmp.$$
            sed -e s,CHOOSE_dir,"$install_dir",g < d.tmp.$$ > d.tmp
	    $mkconfig d.tmp
	fi
	
	if [ "$run_test" ]
	then
	    start $dir-mysql-$clone-$target $dir $res_dir &
	fi
done
cd $p
rm /tmp/filter_hosts.$$

wait

rm -f $LOCK
