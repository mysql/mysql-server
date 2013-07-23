#!/usr/bin/env bash

function usage() {
    echo "run nightly mysql and fractal tree regressions"
    echo "uses gearman to schedule jobs onto test machines"
}

# generate a script that makes a mysql release and run tests on it
function make_and_test_mysql() {
    echo $(date) $* >>$nightlytrace 2>&1
    echo "bash -x \$HOME/github/ft-engine/scripts/tokutek.make.mysql.bash $* >>$mysqltrace 2>&1; \
        buildexitcode=\$?; \
        echo \$(date) \$HOME/github/ft-engine/scripts/tokutek.make.mysql.bash -$* \$buildexitcode >>$mysqltrace; \
        if [ \$buildexitcode -eq 0 ] ; then \$HOME/bin/test.mysql.bash $* >>/tmp/mysql.test.trace 2>&1; fi" \
        | $gearmandir/bin/gearman -b -f mysql-build-$system-$arch -h $gearmandhost -p 4730 >>$nightlytrace 2>&1
}

# make a mysql release
function make_mysql() {
    echo $(date) $* >>$nightlytrace 2>&1
    echo "\$HOME/github/ft-engine/scripts/tokutek.make.mysql.bash $* >>$mysqltrace 2>&1" | $gearmandir/bin/gearman -b -f mysql-build-$system-$arch -h $gearmandhost -p 4730 >>$nightlytrace 2>&1
}

# setup the PATH since cron gives us a minimal PATH
PATH=$HOME/bin:$HOME/usr/local/bin:/usr/local/bin:$PATH
source /etc/profile

github_token=PUT_TOKEN_HERE
gearmandhost=localhost
gearmandir=/usr/local/gearmand-1.1.6
system=$(uname -s | tr '[:upper:]' '[:lower:]')
arch=$(uname -m | tr '[:upper:]' '[:lower:]')
now_ts=$(date +%s)
cc=gcc47
cxx=g++47
ftcc=$cc
ftcxx=$cxx

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

make_and_test_mysql --github_token=$github_token --mysqlbuild=mysql-5.5.30-tokudb-${now_ts}-e-${system}-${arch}
make_and_test_mysql --github_token=$github_token --mysqlbuild=mysql-5.5.30-tokudb-${now_ts}-debug-e-${system}-${arch}

make_and_test_mysql --github_token=$github_token --mysqlbuild=mariadb-5.5.30-tokudb-${now_ts}-${system}-${arch}
make_and_test_mysql --github_token=$github_token --mysqlbuild=mariadb-5.5.30-tokudb-${now_ts}-debug-${system}-${arch}

make_and_test_mysql --github_token=$github_token --mysqlbuild=mysql-5.6.10-tokudb-${now_ts}-${system}-${arch}
make_and_test_mysql --github_token=$github_token --mysqlbuild=mysql-5.6.10-tokudb-${now_ts}-debug-${system}-${arch}

make_and_test_mysql --github_token=$github_token --mysqlbuild=mariadb-5.5.30-tokudb-${now_ts}-e-${system}-${arch}
make_and_test_mysql --github_token=$github_token --mysqlbuild=mariadb-5.5.30-tokudb-${now_ts}-debug-e-${system}-${arch}

# build a test the head of the releases/tokudb-7.0 branch
# make_and_test_mysql --github_use_ssh=1 --mysqlbuild=mysql-5.5.30-tokudb-${now_ts}-70-${system}-${arch} --mysql_tree=releases/tokudb-7.0 --ftengine_tree=releases/tokudb-7.0 --ftindex_tree=releases/tokudb-7.0 --tests=run.mysql.tests.bash:run.sql.bench.bash

exit 0
