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
VERSION="autotest-boot.sh version 1.00"

DATE=`date '+%Y-%m-%d'`
HOST=`hostname -s`
export DATE HOST

set -e

echo "`date` starting: $*"

verbose=0
do_clone=yes
build=yes

tag=
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
                --clone=*) clone=`echo $1 | sed s/--clone=//`;;
                --version) echo $VERSION; exit;;
                --conf=*) conf=`echo $1 | sed s/--conf=//`;;
	        --tag=*) tag=`echo $1 | sed s/--tag=//`;;
	        --*) echo "Unknown arg: $1";;
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

src_clone=${src_clone_base}${clone}

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

if [ -z "$tag" ]
then
    dst_place=${build_dir}/clone-$clone-$DATE.$$
else
    dst_place=${build_dir}/clone-$tag-$DATE.$$
    extra_args="$extra_args --clone=$tag"
    extra_clone="-r$tag"
fi

#########################################
# Delete source and pull down the latest#
#########################################

if [ "$do_clone" ]
then
	rm -rf $dst_place
	if [ `echo $src_clone | grep -c 'file:\/\/'` = 1 ]
	then
		bk clone -l $extra_clone $src_clone $dst_place
	else
		bk clone $extra_clone $src_clone $dst_place
	fi
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
sh -x $script $save_args --conf=$conf --install-dir=$install_dir --suite=$RUN --nolock $extra_args

if [ "$build" ]
then
    rm -rf $dst_place
fi
rm -f $LOCK
