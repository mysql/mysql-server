#!/bin/sh
#############################################################
# This script created by Jonas does the following	    #
# Cleans up clones and pevious builds, pulls new clones,    #
# builds, deploys, configures the tests and launches ATRT   #
#############################################################

###############
#Script setup #
##############

save_args=$*
VERSION="autotest-boot.sh version 1.00"

DATE=`date '+%Y-%m-%d'`
HOST=`hostname -s`
export DATE HOST

set -e

echo "`date` starting: $*"

verbose=0
do_clone=yes
build=yes

conf=
LOCK=$HOME/.autotest-lock

############################
# Read command line entries#
############################

while [ "$1" ]
do
        case "$1" in
                --no-clone) do_clone="";;
                --no-build) build="";;
                --verbose) verbose=`expr $verbose + 1`;;
                --clone=*) clone=`echo $1 | sed s/--clone=//`;;
                --version) echo $VERSION; exit;;
                --conf=*) conf=`echo $1 | sed s/--conf=//`;;
                *) RUN=$*;;
        esac
        shift
done

#################################
#Make sure the configfile exists#
#if it does not exit. if it does#
# (.) load it			# 
#################################
if [ -z "$conf" ]
then
    conf=`pwd`/autotest.conf
fi

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
vars="src_clone_base install_dir build_dir"
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

dst_place=${build_dir}/clone-mysql-$clone-$DATE.$$

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
        rm -rf $install_dir
	BUILD/compile-ndb-autotest --prefix=$install_dir
	make install
fi


################################
# Start run script             #
################################

script=$install_dir/mysql-test/ndb/autotest-run.sh
$script $save_args --conf=$conf --install-dir=$install_dir --suite=$RUN --nolock

if [ "$build" ]
then
    rm -rf $dst_place
fi
rm -f $LOCK
