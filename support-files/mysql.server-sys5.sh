#!/bin/sh
#
# This is an example SysV-init-script by Winfried Truemper that you can use
# and modify to your liking
#

PATH="$PATH:@prefix@"
export PATH
MY_CFG="@prefix@/mysql.cfg"

read_mysql_config() {
    # this routine requires a sed, which reads even the last line of input

    MY_CONFIG_FILE="$1"    # file to read setting from
    MY_CONFIG_SECTION="$2" # section inside the file
    MY_CONFIG_TAG="$3"     # name of the setting inside the section
    TAB=`printf "\t" ""`   # makes the code cut&paste safe

    sed -n -f - "$MY_CONFIG_FILE" <<EOF
1,/^\[$MY_CONFIG_SECTION\]/ d
/^\[[a-z]/ q
/^$MY_CONFIG_TAG/ {
s/^$MY_CONFIG_TAG[ $TAB]*=[ $TAB]*\([^ $TAB]*\)/\1/
p
q
}
EOF
}



do_start() {
    nohup ./bin/mysqld --defaults-file="$MY_CFG" &
}

do_stop() {
    ./bin/mysqladmin --defaults-file="$MY_CFG" shutdown
}

do_kill_all() {
    PIDS=`ps -efo pid,args | grep mysql | sed -e "s,  *.*,," | sort | uniq`
    kill $PIDS
    sleep 5
    kill -9 $PIDS
}

do_kill() {
    MY_PIDFILE=`read_mysql_config "$MY_CFG" "mysqld" "pidfile" `
    read MY_PID < "$MY_PIDFILE"
    kill "$MY_PID"
    sleep 2
    kill -KILL "$MY_PID"
}

# z.B. mysql.sh admin "ping"
do_admin() {
    shift
    ./bin/mysqladmin --defaults-file="$MY_CFG" $@
    exit
}

do_repair() {
    MY_DATADIR=`read_mysql_config "$MY_CFG" "mysqld" "datadir" `
    ./bin/isamchk --defaults-file="$MY_CFG" --repair "$MY_DATADIR/$1"
    shift
}


do_repair_all() {
    MY_DATADIR=`read_mysql_config "$MY_CFG" "mysqld" "datadir" `
    for i in `find "$MY_DATADIR" -name "*.ISM"`
    do
        ./bin/isamchk --defaults-file="$MY_CFG" --repair "$MY_DATADIR/$i"
    done
}



MY_BASEDIR=`read_mysql_config "$MY_CFG" "mysqld" "basedir"`
cd "$MY_BASEDIR" || exit 1
while test $# -gt 0
do
    MY_ARG="$1"
    do_$MY_ARG $@
    shift
done
