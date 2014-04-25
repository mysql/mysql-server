#!/usr/bin/env bash

function usage() {
    echo "run nightly mysql and fractal tree regressions"
    echo "uses gearman to schedule jobs onto test machines"
}

# generate a script that makes a mysql release and run tests on it
function make_and_test_mysql() {
    echo $(date) $* >>$nightlytrace 2>&1
    echo "bash -x \$HOME/github/tokudb-engine/scripts/tokutek.make.mysql.bash $* >>$mysqltrace 2>&1; \
        buildexitcode=\$?; \
        echo \$(date) \$HOME/github/tokudb-engine/scripts/tokutek.make.mysql.bash -$* \$buildexitcode >>$mysqltrace; \
        if [ \$buildexitcode -eq 0 ] ; then \$HOME/bin/test.mysql.bash $* >>/tmp/mysql.test.trace 2>&1; fi" \
        | $gearmandir/bin/gearman -b -f mysql-build-$system-$arch -h $gearmandhost -p 4730 >>$nightlytrace 2>&1
}

# make a mysql release
function make_mysql() {
    echo $(date) $* >>$nightlytrace 2>&1
    echo "\$HOME/github/tokudb-engine/scripts/tokutek.make.mysql.bash $* >>$mysqltrace 2>&1" | $gearmandir/bin/gearman -b -f mysql-build-$system-$arch -h $gearmandhost -p 4730 >>$nightlytrace 2>&1
}

# setup the PATH since cron gives us a minimal PATH
PATH=$HOME/bin:$HOME/usr/local/bin:/usr/local/bin:$PATH
source /etc/profile

github_token=
gearmandhost=localhost
gearmandir=/usr/local/gearmand-1.1.6
system=$(uname -s | tr '[:upper:]' '[:lower:]')
arch=$(uname -m | tr '[:upper:]' '[:lower:]')
now_ts=$(date +%s)
cc=gcc
cxx=g++

while [ $# -gt 0 ] ; do
    arg=$1; shift
    if [[ $arg =~ --(.*)=(.*) ]] ; then
        eval ${BASH_REMATCH[1]}=${BASH_REMATCH[2]}
    else
        usage; exit 1;
    fi
done

nightlytrace=/tmp/$(whoami).nightly.trace
mysqltrace=/tmp/$(whoami).mysql.build.trace.$now_ts

make_and_test_mysql --mysqlbuild=mysql-5.6.16-tokudb-${now_ts}-debug-e-${system}-${arch} --cc=$cc --cxx=$cxx --github_token=$github_token 
make_and_test_mysql --mysqlbuild=mysql-5.6.16-tokudb-${now_ts}-e-${system}-${arch} --cc=$cc --cxx=$cxx --github_token=$github_token --tests=run.mysql.tests.bash:run.sql.bench.bash 

make_and_test_mysql --mysqlbuild=mysql-5.5.36-tokudb-${now_ts}-debug-e-${system}-${arch} --cc=$cc --cxx=$cxx --github_token=$github_token 
make_and_test_mysql --mysqlbuild=mysql-5.5.36-tokudb-${now_ts}-e-${system}-${arch} --cc=$cc --cxx=$cxx --github_token=$github_token --tests=run.mysql.tests.bash:run.sql.bench.bash

make_and_test_mysql --mysqlbuild=mariadb-5.5.35-tokudb-${now_ts}-debug-e-${system}-${arch} --cc=$cc --cxx=$cxx --github_token=$github_token 
make_and_test_mysql --mysqlbuild=mariadb-5.5.35-tokudb-${now_ts}-e-${system}-${arch} --cc=$cc --cxx=$cxx --github_token=$github_token  --tests=run.mysql.tests.bash:run.sql.bench.bash

exit 0
