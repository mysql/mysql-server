#!/bin/sh

# Copyright (C) 2003, 2005 MySQL AB
# Use is subject to license terms
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

baseport=""
basedir=""
proc_no=1
node_id=1

d_file=/tmp/d.$$
dir_file=/tmp/dirs.$$
config_file=/tmp/config.$$
cluster_file=/tmp/cluster.$$

add_procs(){
	type=$1; shift
	while [ $# -ne 0 ]
	do
		add_proc $type $1
		shift
	done
}

add_proc (){
	case $type in
	mgm)
		echo "$proc_no.ndb_mgmd" >> $dir_file
		echo "[ndb_mgmd]"        >> $config_file
		echo "Id: $node_id"      >> $config_file
		echo "HostName: $2"      >> $config_file
		node_id=`expr $node_id + 1`
		;;
	api)
		echo "$proc_no.ndb_api" >> $dir_file
                echo "[api]"            >> $config_file
                echo "Id: $node_id"     >> $config_file
                echo "HostName: $2"     >> $config_file
		node_id=`expr $node_id + 1`
		;;
	ndb)
		echo "$proc_no.ndbd" >> $dir_file
                echo "[ndbd]"        >> $config_file
                echo "Id: $node_id"  >> $config_file
                echo "HostName: $2"  >> $config_file
		node_id=`expr $node_id + 1`
		;;
	mysqld)
		echo "$proc_no.mysqld" >> $dir_file
                echo "[mysqld]"      >> $config_file
                echo "Id: $node_id"    >> $config_file
                echo "HostName: $2"    >> $config_file
		node_id=`expr $node_id + 1`
		;;
	mysql)
		echo "$proc_no.mysql" >> $dir_file
		;;
	esac
	proc_no=`expr $proc_no + 1`
}


cnf=/dev/null
cat $1 | while read line
do
	case $line in
	baseport:*) baseport=`echo $line | sed 's/baseport[ ]*:[ ]*//g'`;;
	basedir:*) basedir=`echo $line | sed 's/basedir[ ]*:[ ]*//g'`;;
	mgm:*) add_procs mgm `echo $line | sed 's/mgm[ ]*:[ ]*//g'`;;
	api:*) add_procs api `echo $line | sed 's/api[ ]*:[ ]*//g'`;;
	ndb:*) add_procs ndb `echo $line | sed 's/ndb[ ]*:[ ]*//g'`;;
	mysqld:*) add_procs mysqld `echo $line | sed 's/mysqld[ ]*:[ ]*//g'`;;
	mysql:*) add_procs mysql `echo $line | sed 's/mysql[ ]*:[ ]*//g'`;;
	"-- cluster config") 
		if [ "$cnf" = "/dev/null" ]
		    then
		    cnf=$cluster_file
		else
		    cnf=/dev/null
		fi
		line="";;
	    *) echo $line >> $cnf; line="";;
	esac
	if [ "$line" ]
	then
	    echo $line >> $d_file
	fi
done

cat $dir_file | xargs mkdir -p

if [ -f $cluster_file ]
    then
    cat $cluster_file $config_file >> /tmp/config2.$$
    mv /tmp/config2.$$ $config_file
fi

for i in `find . -type d -name '*.ndb_mgmd'`
  do
  cp $config_file $i/config.ini
done

mv $d_file d.txt
rm -f $config_file $dir_file $cluster_file
