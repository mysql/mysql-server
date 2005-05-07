#!/bin/sh

save_args=$*
VERSION="ndb-autotest.sh version 1.04"

DATE=`date '+%Y-%m-%d'`
export DATE

set -e
ulimit -Sc unlimited

echo "`date` starting: $*"

RSYNC_RSH=ssh
export RSYNC_RSH

do_clone=yes
build=yes
deploy=yes

clone=5.0-ndb
RUN="daily-basic daily-devel"
conf=autotest.conf

while [ "$1" ]
do
        case "$1" in
                --no-clone) do_clone="";;
                --no-build) build="";;
                --no-deploy) deploy="";;
		--clone=*) clone=`echo $1 | sed s/--clone=//`;;
                --conf=*) conf=`echo $1 | sed s/--conf=//`;;
                --version) echo $VERSION; exit;;
                *) RUN=$*;;
        esac
        shift
done

if [ -f $conf ]
then
	. $conf
else
	echo "Can't find config file: $conf"
	exit
fi

env

LOCK=$HOME/.autotest-lock
src_clone=$src_clone_base-$clone

if [ -f $LOCK ]
then
	echo "Lock file exists: $LOCK"
	exit 1
fi

echo "$DATE $RUN" > $LOCK
trap "rm -f $LOCK" ERR

dst_place=${build_dir}/clone-mysql-$clone-$DATE

if [ "$do_clone" ]
then
	rm -rf $dst_place
	bk clone  $src_clone $dst_place
fi

if [ "$build" ]
then
	cd $dst_place
        rm -rf $run_dir/*
        aclocal; autoheader; autoconf; automake
	if [ -d storage ]
	then
	    (cd storage/innobase; aclocal; autoheader; autoconf; automake)
	    (cd storage/bdb/dist; sh s_all)
	else
	    (cd innobase; aclocal; autoheader; autoconf; automake)
	    (cd bdb/dist; sh s_all)
	fi
	eval $configure --prefix=$run_dir
	make
	make install
	(cd $run_dir; ./bin/mysql_install_db)
fi

###
# check script version
#
script=$run_dir/mysql-test/ndb/ndb-autotest.sh
if [ -x $script ]
then
	$script --version > /tmp/version.$$
else
	echo $VERSION > /tmp/version.$$
fi	
match=`grep -c "$VERSION" /tmp/version.$$`
rm -f /tmp/version.$$
if [ $match -eq 0 ]
then
	echo "Incorrect script version...restarting"
	cp $run_dir/mysql-test/ndb/ndb-autotest.sh /tmp/at.$$.sh
	rm -rf $run_dir $dst_place
	sh /tmp/at.$$.sh $save_args
	exit
fi

# Check that all interesting files are present
test_dir=$run_dir/mysql-test/ndb
atrt=$test_dir/atrt
html=$test_dir/make-html-reports.sh
mkconfig=$run_dir/mysql-test/ndb/make-config.sh

PATH=$run_dir/bin:$test_dir:$PATH
export PATH

filter(){
	neg=$1
	shift
	while [ $# -gt 0 ]
	do
		if [ `grep -c $1 $neg` -eq 0 ] ; then echo $1; fi
		shift
	done
}

###
# check ndb_cpcc fail hosts
#
ndb_cpcc $hosts | awk '{ if($1=="Failed"){ print;}}' > /tmp/failed.$DATE
filter /tmp/failed.$DATE $hosts > /tmp/hosts.$DATE
hosts=`cat /tmp/hosts.$DATE` 

if [ "$deploy" ]
then
    for i in $hosts
      do
      rsync -a --delete --force --ignore-errors $run_dir/ $i:$run_dir
      ok=$?
      if [ $ok -ne 0 ]
	  then
	  echo "$i failed during rsync, excluding"
	  echo $i >> /tmp/failed.$DATE
      fi
    done
fi
rm -f /tmp/build.$DATE.tgz

###
# handle scp failed hosts
#
filter /tmp/failed.$DATE $hosts > /tmp/hosts.$DATE
hosts=`cat /tmp/hosts.$DATE` 
cat /tmp/failed.$DATE > /tmp/filter_hosts.$$

###
# functions for running atrt 
#
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
    host=`hostname -s`
    if [ -f $test_dir/conf-$1-$host.txt ]
    then
	echo "$test_dir/conf-$1-$host.txt"
    elif [ -f $test_dir/conf-$1.txt ]
    then
	echo "$test_dir/conf-$1.txt"
    fi
}

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
	tar cfz /tmp/res.$$.tgz `basename $p2`/$DATE
	scp /tmp/res.$$.tgz $result_host:$result_path/res.$DATE.`hostname -s`.$$.tgz
	rm -f /tmp/res.$$.tgz
}

count_hosts(){
    cnt=`grep "CHOOSE_host" $1 |
      awk '{for(i=1; i<=NF;i++) if(match($i, "CHOOSE_host") > 0) print $i;}' |
      sort | uniq | wc -l`
    echo $cnt
}

p=`pwd`
for dir in $RUN
do
	echo "Fixing hosts for $dir"

	run_dir=$base_dir/run-$dir-mysql-$clone-$target
	res_dir=$base_dir/result-$dir-mysql-$clone-$target/$DATE

	mkdir -p $run_dir $res_dir
	rm -rf $res_dir/* $run_dir/*
	
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

	run_hosts=`echo $avail_hosts|awk '{for(i=1;i<='$count';i++)print $i;}'`
	echo $run_hosts >> /tmp/filter_hosts.$$	
	
	cd $run_dir
	choose $conf $run_hosts > d.tmp
	$mkconfig d.tmp
	start $dir-mysql-$clone-$target $dir $res_dir &
done
cd $p
rm /tmp/filter_hosts.$$

wait

rm -f $LOCK
