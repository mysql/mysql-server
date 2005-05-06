#!/bin/sh

baseport=""
basedir=""
proc_no=1
node_id=1

dir_file=/tmp/dirs.$$
config_file=/tmp/config.$$

add_procs(){
	type=$1; shift
	while [ $# -ne 0 ]
	do
		add_proc $type $1
		shift
	done
}

add_proc (){
	dir=""
	conf=""
	case $type in
	mgm)
		dir="ndb_mgmd"
		conf="[ndb_mgmd]\nId: $node_id\nHostName: $2\n"
		node_id=`expr $node_id + 1`
		;;
	api)
		dir="ndb_api"
		conf="[api]\nId: $node_id\nHostName: $2\n"
		node_id=`expr $node_id + 1`
		;;
	ndb)
		dir="ndb"
		conf="[ndbd]\nId: $node_id\nHostName: $2\n"
		node_id=`expr $node_id + 1`
		;;
	mysqld)
		dir="mysqld"
		conf="[mysqld]\nId: $node_id\nHostName: $2\n"
		node_id=`expr $node_id + 1`
		;;
	mysql)
		dir="mysql"
		;;
	esac
	dir="$proc_no.$dir"
	proc_no=`expr $proc_no + 1`
	echo -e $dir >> $dir_file
	if [ "$conf" ]
	then
		echo -e $conf >> $config_file
	fi
}


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
	esac
done

cat $dir_file | xargs mkdir

if [ "$2" ]
then
    cat $2 $config_file >> /tmp/config2.$$
    mv /tmp/config2.$$ $config_file
fi

for i in `find . -type d -name '*.ndb_mgmd'`
do
  cp $config_file $i/config.ini
done

rm -f $config_file $dir_file
