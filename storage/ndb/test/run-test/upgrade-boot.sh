#!/bin/sh

# Copyright (c) 2007, 2012, Oracle and/or its affiliates. All rights reserved.
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
VERSION="upgrade-boot.sh version 1.00"

DATE=`date '+%Y-%m-%d'`
HOST=`hostname -s`
export DATE HOST

set -e

echo "`date` starting: $*"

verbose=0
do_clone=yes
build=yes

tag0=
tag1=
conf=
extra_args=
extra_clone=
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
                --clone=*) clone0=`echo $1 | sed s/--clone=//`;;
                --clone0=*) clone0=`echo $1 | sed s/--clone0=//`;;
                --clone1=*) clone1=`echo $1 | sed s/--clone1=//`;;
                --version) echo $VERSION; exit;;
                --conf=*) conf=`echo $1 | sed s/--conf=//`;;
	        --tag=*) tag0=`echo $1 | sed s/--tag=//`;;
	        --tag0=*) tag0=`echo $1 | sed s/--tag0=//`;;
	        --tag1=*) tag1=`echo $1 | sed s/--tag1=//`;;
	        --*) echo "Unknown arg: $1";;
                *) RUN=$*;;
        esac
        shift
done

if [ -z "$clone1" ]
then
	clone1=$clone0
fi

if [ -z "$tag0" ]
then
	echo "No tag0 specified"
	exit
fi

if [ -z "$tag1" ]
then
        echo "No tag1 specified"
        exit
fi

#################################
#Make sure the configfile exists#
#if it does not exit. if it does#
# (.) load it			# 
#################################
if [ -z "$conf" ]
then
	if [ -f "`pwd`/autotest.conf" ]
	then
		conf="`pwd`/autotest.conf"
	elif [ -f "$HOME/autotest.conf" ]
	then
		conf="$HOME/autotest.conf"
	fi
fi

if [ -f $conf ]
then
	. $conf
else
	echo "Can't find config file: >$conf<"
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

src_clone0=${src_clone_base}${clone0}
src_clone1=${src_clone_base}${clone1}

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

dst_place0=${build_dir}/clone-$tag0-$DATE.$$
dst_place1=${build_dir}/clone-$tag1-$DATE.$$

#########################################
# Delete source and pull down the latest#
#########################################

if [ "$do_clone" ]
then
	rm -rf $dst_place0 $dst_place1
	if [ `echo $src_clone0 | grep -c 'file:\/\/'` = 1 ]
	then
		bk clone -l -r$tag0 $src_clone0 $dst_place0
	else
		bk clone -r$tag0 $src_clone0 $dst_place0
	fi

        if [ `echo $src_clone1 | grep -c 'file:\/\/'` = 1 ]
        then
                bk clone -l -r$tag1 $src_clone1 $dst_place1
        else
                bk clone -r$tag1 $src_clone1 $dst_place1
        fi
fi

##########################################
# Build the source, make installs, and   #
# create the database to be rsynced	 #
##########################################
function build_cluster()
{
    if [ -x storage/ndb/compile-cluster ]
    then
        storage/ndb/compile-cluster --autotest $*
    else
        BUILD/compile-ndb-autotest $*
    fi
}

install_dir0=$install_dir/$tag0
install_dir1=$install_dir/$tag1
if [ "$build" ]
then
	cd $dst_place0
        rm -rf $install_dir0
        build_cluster --prefix=$install_dir0
	make install

	cd $dst_place1
	rm -rf $install_dir1
        build_cluster --prefix=$install_dir1
	make install
        fi
fi


################################
# Start run script             #
################################

script=$install_dir1/mysql-test/ndb/upgrade-run.sh
$script $save_args --conf=$conf --install-dir=$install_dir --suite=$RUN --nolock $extra_args

if [ "$build" ]
then
    rm -rf $dst_place0 $dst_place1
fi
rm -f $LOCK
