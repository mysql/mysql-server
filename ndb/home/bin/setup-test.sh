#!/bin/sh

# NAME
#   run-test.sh - Run a test program 
# 
# SYNOPSIS
#   setup-test.sh [ -n <ndb dir>] [ -r <run dir>] 
#
# DESCRIPTION
#  run a test
#
# OPTIONS
#
# EXAMPLES
#   
# ENVIRONMENT
#   NDB_PROJ_HOME       Home dir for ndb
#   
# FILES
#   $NDB_PROJ_HOME/lib/funcs.sh  	shell script functions
#
# DIAGNOSTICTS
#   
# VERSION   
#   1.01
#   
# AUTHOR
#   Jonas Oreland
#
# 

progname=`basename $0`
synopsis="setup-test.sh [-x xterm] [ -n <ndb dir>] [ -r <run dir>]"

: ${NDB_PROJ_HOME:?}		 # If undefined, exit with error message

: ${RUN_NDB_NODE_OPTIONS:=--}    # If undef, set to --. Keeps getopts happy.
			         # You may have to experiment a bit  
                                 # to get quoting right (if you need it).


. $NDB_PROJ_HOME/lib/funcs.sh    # Load some good stuff

# defaults for options related variables 
#

verbose=yes
options=""
ndb_dir=$NDB_TOP
if [ -z "$ndb_dir" ]
then
    ndb_dir=`pwd`
fi

local_dir=`pwd`
own_host=`hostname`
uniq_id=$$.$$

_xterm=$XTERM
_rlogin="ssh -X"

# used if error when parsing the options environment variable
#
env_opterr="options environment variable: <<$options>>" 


# Option parsing, for the options variable as well as the command line.
#
# We want to be able to set options in an environment variable,
# as well as on the command line. In order not to have to repeat
# the same getopts information twice, we loop two times over the
# getopts while loop. The first time, we process options from
# the options environment variable, the second time we process 
# options from the command line.
#
# The things to change are the actual options and what they do.
#
#
for optstring in "$options"  ""	     # 1. options variable  2. cmd line
do
    while getopts n:r:x: i $optstring  # optstring empty => no arg => cmd line
    do
	case $i in

	n)      ndb_dir=$OPTARG;;	# Ndb dir
	r)      run_dir=$OPTARG;;	# Run dir
	x)      _xterm=$OPTARG;;        
	\?)	syndie $env_opterr;;    # print synopsis and exit

	esac
    done

    [ -n "$optstring" ]  &&  OPTIND=1 	# Reset for round 2, cmdline options

    env_opterr= 			# Round 2 should not use the value

done
shift `expr $OPTIND - 1`

# --- option parsing done ---

ndb_dir=`abspath $ndb_dir`
run_dir=`abspath $run_dir`

trace "Verifying arguments"

if [ ! -d $ndb_dir/bin ] || [ ! -d $ndb_dir/lib ]
then
    msg "Ndb home path seems incorrect either $ndb_dir/bin or $ndb_dir/lib not found"
    exit 1004
fi

ndb_bin=$ndb_dir/bin/ndb
mgm_bin=$ndb_dir/bin/mgmtsrvr
api_lib=$ndb_dir/lib/libNDB_API.so

if [ ! -x $ndb_bin ]
then
    msg "Ndb path seems incorrect ndb binary not found: $ndb_bin"
    exit 1004
fi

if [ ! -x $mgm_bin ]
then
    msg "Ndb path seems incorrect management server binary not found: $mgm_bin"
    exit 1004
fi

init_config=$run_dir/mgm.1/initconfig.txt
local_config=$run_dir/mgm.1/localcfg.txt
if [ ! -r $init_config ] || [ ! -r $local_config ]
then
    msg "Run path seems incorrect $init_config or $local_config not found"
    exit 1004
fi

trace "Parsing $init_config"
awk -f $NDB_PROJ_HOME/bin/parseConfigFile.awk $init_config > /tmp/run-test.$uniq_id
. /tmp/run-test.$uniq_id
cat /tmp/run-test.$uniq_id
rm -f /tmp/run-test.$uniq_id

trace "Parsing $local_config"
MgmPort=`grep -v "OwnProcessId" $local_config | cut -d " " -f 2`

trace "Verifying that mgm port is empty"
telnet $mgm_1 $MgmPort > /tmp/mgm_port.$uniq_id 2>&1 <<EOF
EOF

if [ 0 -lt `grep -c -i connected /tmp/mgm_port.$uniq_id` ]
then
    rm /tmp/mgm_port.$uniq_id
    msg "There is already something using port $mgm_1:$MgmPort"
    exit 1003
fi
rm /tmp/mgm_port.$uniq_id

fixhost(){
    if [ "$1" != localhost ]
    then
	echo $1
    else
        uname -n
    fi
}

do_xterm(){
    title=$1
    shift
    xterm -fg black -title "$title" -e $*
}

save_profile(){
    cp $HOME/.profile /tmp/.profile.$uniq_id
}

wait_restore_profile(){
    while [ -r /tmp/.profile.$uniq_id ]
    do
	sleep 1
    done
}

start_mgm(){
    trace "Starting Management server on: $mgm_1"
    save_profile
    mgm_1=`fixhost $mgm_1`

    ( 
	echo "PATH=$ndb_dir/bin:\$PATH"
	echo "LD_LIBRARY_PATH=$ndb_dir/lib:\$LD_LIBRARY_PATH"
	echo "export PATH LD_LIBRARY_PATH"
	echo "cd $run_dir/mgm.1"
	echo "ulimit -Sc unlimited"
        echo "mv /tmp/.profile.$uniq_id $HOME/.profile"
    ) >> $HOME/.profile
    do_xterm "Mmg on $mgm_1" ${_rlogin} $mgm_1 &
    wait_restore_profile
}

start_ndb_node(){
    node_id=$1
    dir=$run_dir/ndb.$1
    ndb_host=`eval echo "\$"ndb_$node_id`
    ndb_host=`fixhost $ndb_host`
    ndb_fs=`eval echo "\$"ndbfs_$node_id`

    trace "Starting Ndb node $node_id on $ndb_host"
    save_profile

    ( 
	echo "PATH=$ndb_dir/bin:\$PATH"
	echo "LD_LIBRARY_PATH=$ndb_dir/lib:\$LD_LIBRARY_PATH"
	echo "mkdir -p $ndb_fs"
	echo "export PATH LD_LIBRARY_PATH"
	echo "cd $dir"
	echo "ulimit -Sc unlimited"
        echo "mv /tmp/.profile.$uniq_id $HOME/.profile"
    ) >> $HOME/.profile
    do_xterm "Ndb: $node_id on $ndb_host" ${_rlogin} $ndb_host &
    wait_restore_profile
}

start_api_node(){
    node_id=$1
    dir=$run_dir/api.$1
    api_host=`eval echo "\$"api_$node_id`
    api_host=`fixhost $api_host`

    trace "Starting api node $node_id on $api_host"
    save_profile

    ( 
	echo "PATH=$ndb_dir/bin:\$PATH"
	echo "LD_LIBRARY_PATH=$ndb_dir/lib:\$LD_LIBRARY_PATH"
	echo "export PATH LD_LIBRARY_PATH NDB_PROJ_HOME"
	echo "cd $dir"
	echo "ulimit -Sc unlimited"
        echo "mv /tmp/.profile.$uniq_id $HOME/.profile"
    ) >> $HOME/.profile
    do_xterm "API: $node_id on $api_host" ${_rlogin} $api_host &
    wait_restore_profile
}

for_each_ndb_node(){
    i=1
    j=`expr $mgm_nodes + 1`
    while [ $i -le $ndb_nodes ]
    do
	$* $j
	j=`expr $j + 1`
	i=`expr $i + 1`
    done
}

for_each_api_node(){
    i=1
    j=`expr $mgm_nodes + $ndb_nodes + 1`
    while [ $i -le $api_nodes ]
    do
	$* $j
	j=`expr $j + 1`
	i=`expr $i + 1`
    done
}

start_mgm
for_each_ndb_node start_ndb_node
for_each_api_node start_api_node

exit 0

