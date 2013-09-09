#!/usr/bin/env bash

function usage() {
    echo "run the mysql tests"
    echo "--mysqlbuild=$mysqlbuild --tests=$tests"
}

function expand() {
    echo $* | tr ,: " "
}

mysqlbuild=
mysqlsocket=/tmp/mysql.sock
gearmandir=/usr/local/gearmand-1.1.6
gearmandhost=localhost
system=$(uname -s | tr [:upper:] [:lower:])
arch=$(uname -m | tr [:upper:] [:lower:])
tests=run.mysql.tests.bash

while [ $# -gt 0 ] ; do
    arg=$1; shift
    if [[ $arg =~ --(.*)=(.*) ]] ; then
	eval ${BASH_REMATCH[1]}=${BASH_REMATCH[2]}
    else
	usage; exit 1;
    fi
done

if [ -z $mysqlbuild ] ; then exit 1; fi

for testname in $(expand $tests) ; do
    if [ $testname = "run.mysql.tests.bash" ] ; then
	run_mysqld=0
    else
	run_mysqld=1
    fi
    if [ $run_mysqld = 0 ] ; then
	setupextra="--shutdown=1 --install=1 --startup=0"
    else
	setupextra="--shutdown=1 --install=1 --startup=1"
    fi
    echo "echo \$(date) $mysqlbuild >>/tmp/$(whoami).$testname.trace 2>&1; \
          \$HOME/bin/setup.mysql.bash --mysqlbuild=$mysqlbuild $setupextra >>/tmp/$(whoami).$testname.trace 2>&1; \
          testexitcode=\$?; \
          echo \$(date) $mysqlbuild \$testexitcode >>/tmp/$(whoami).$testname.trace 2>&1; \
          if [ \$testexitcode -ne 0 ] ; then exit 1; fi; \
          \$HOME/bin/$testname --mysqlbuild=$mysqlbuild --commit=1 >>/tmp/$(whoami).$testname.trace 2>&1; \
	  if [ $run_mysqld != 0 ] ; then mysqladmin -S$mysqlsocket shutdown; fi" | $gearmandir/bin/gearman -b -f mysql-test-$system-$arch -h $gearmandhost -p 4730
done

exit 0
