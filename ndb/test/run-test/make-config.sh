#!/bin/sh
# NAME
#   make-config.sh - Makes a config file for mgm server
#
# SYNOPSIS
#   make-config.sh [ -t <template> ] [-s] [ -m <machine conf> [ -d <directory> ]
#
# DESCRIPTION
#
# OPTIONS
#
# EXAMPLES
#   
#   
# ENVIRONMENT
#   NDB_PROJ_HOME       Home dir for ndb
#   
# FILES
#   $NDB_PROJ_HOME/lib/funcs.sh  general shell script functions
#
#
# SEE ALSO
#   
# DIAGNOSTICTS
#   
# VERSION   
#   1.0
#   1.1 021112 epesson: Adapted for new mgmt server in NDB 2.00
#   
# AUTHOR
#   Jonas Oreland
#
# CHANGES
#   also generate ndbnet config
#

progname=`basename $0`
synopsis="make-config.sh [ -t template ] [ -m <machine conf> ] [ -d <dst directory> ][-s] [<mgm host>]"

#: ${NDB_PROJ_HOME:?}		 # If undefined, exit with error message

#: ${NDB_LOCAL_BUILD_OPTIONS:=--} # If undef, set to --. Keeps getopts happy.
			         # You may have to experiment a bit  
                                 # to get quoting right (if you need it).


#. $NDB_PROJ_HOME/lib/funcs.sh    # Load some good stuff
trace() {
        echo $* 1>&2
}
syndie() {
        trace $*
        exit 1
}

# defaults for options related variables 
#

mgm_nodes=0
ndb_nodes=0
api_nodes=0
uniq_id=$$.$$
own_host=`hostname`
dst_dir=""
template=/dev/null
machines=/dev/null
verbose=yes

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
add_node(){
	no=$1; shift
	type=$1; shift
	echo $* | awk 'BEGIN{FS=":";}{h=$1; if(h=="localhost") h="'$own_host'";
                       printf("%s_%d_host=%s\n", "'$type'", "'$no'", h); 
                       if(NF>1 && $2!="") printf("%s_%d_port=%d\n", 
                         "'$type'", "'$no'", $2);
                       if(NF>2 && $3!="") printf("%s_%d_dir=%s\n",
                         "'$type'", "'$no'", $3);
                       }'
}


add_mgm_node(){
    mgm_nodes=`cat /tmp/mgm_nodes.$uniq_id | grep "_host=" | wc -l`
    mgm_nodes=`expr $mgm_nodes + 1`
    while [ $# -gt 0 ]
    do
	add_node ${mgm_nodes} mgm_node $1 >> /tmp/mgm_nodes.$uniq_id
	shift
	mgm_nodes=`expr $mgm_nodes + 1`
    done
}

add_ndb_node(){
    ndb_nodes=`cat /tmp/ndb_nodes.$uniq_id | grep "_host=" | wc -l`
    ndb_nodes=`expr $ndb_nodes + 1`
    while [ $# -gt 0 ]
    do
	add_node ${ndb_nodes} ndb_node $1 >> /tmp/ndb_nodes.$uniq_id
	shift
	ndb_nodes=`expr $ndb_nodes + 1`
    done
}

add_api_node(){
    api_nodes=`cat /tmp/api_nodes.$uniq_id | grep "_host=" |wc -l`
    api_nodes=`expr $api_nodes + 1`
    while [ $# -gt 0 ]
    do
	add_node ${api_nodes} api_node $1 >> /tmp/api_nodes.$uniq_id
	shift
	api_nodes=`expr $api_nodes + 1`
    done
}

rm -rf /tmp/mgm_nodes.$uniq_id ; touch /tmp/mgm_nodes.$uniq_id
rm -rf /tmp/ndb_nodes.$uniq_id ; touch /tmp/ndb_nodes.$uniq_id
rm -rf /tmp/api_nodes.$uniq_id ; touch /tmp/api_nodes.$uniq_id

for optstring in "$options"  ""	     # 1. options variable  2. cmd line
do

    while getopts d:m:t:n:o:a:b:p:s i $optstring  # optstring empty => no arg => cmd line
    do
	case $i in

	q)	verbose="";;		# echo important things
	t)      template=$OPTARG;;      # Template
	d)      dst_dir=$OPTARG;;       # Destination directory
	m)      machines=$OPTARG;;      # Machine configuration
	s)      mgm_start=yes;;         # Make mgm start script
	\?)	syndie $env_opterr;;    # print synopsis and exit

	esac
    done

    [ -n "$optstring" ]  &&  OPTIND=1 	# Reset for round 2, cmdline options

    env_opterr= 			# Round 2 should not use the value

done
shift `expr $OPTIND - 1`

if [ -z "$dst_dir" ]
then
    verbose=
fi

skip(){
	no=$1; shift
	shift $no
	echo $*
}

# --- option parsing done ---
grep "^ndb: " $machines | while read node
do
    node=`skip 1 $node`
    add_ndb_node $node
done

grep "^api: " $machines | while read node
do
    node=`skip 1 $node`
    add_api_node $node
done

grep "^mgm: " $machines | while read node
do
    node=`skip 1 $node` 
    add_mgm_node $node
done

tmp=`grep "^baseport: " $machines | tail -1 | cut -d ":" -f 2`
if [ "$tmp" ]
then 
	baseport=`echo $tmp`
else
	syndie "Unable to find baseport"
fi

trim(){
    echo $*
}
tmp=`grep "^basedir: " $machines | tail -1 | cut -d ":" -f 2`
if [ "$tmp" ]
then 
        basedir=`trim $tmp`
fi

# -- Load enviroment --
ndb_nodes=`cat /tmp/ndb_nodes.$uniq_id | grep "_host=" | wc -l`
api_nodes=`cat /tmp/api_nodes.$uniq_id | grep "_host=" | wc -l`
mgm_nodes=`cat /tmp/mgm_nodes.$uniq_id | grep "_host=" | wc -l`
. /tmp/ndb_nodes.$uniq_id
. /tmp/api_nodes.$uniq_id
. /tmp/mgm_nodes.$uniq_id
rm -f /tmp/ndb_nodes.$uniq_id /tmp/api_nodes.$uniq_id /tmp/mgm_nodes.$uniq_id

# -- Verify
trace "Verifying arguments"

if [ ! -r $template ]
then
    syndie "Unable to read template file: $template"
fi

if [ $ndb_nodes -le 0 ]
then
    syndie "No ndb nodes specified"
fi

if [ $api_nodes -le 0 ]
then
    syndie "No api nodes specified"
fi

if [ $mgm_nodes -gt 1 ]
then
    syndie "More than one mgm node specified"
fi

if [ $mgm_nodes -eq 0 ]
then
    trace "No managment server specified using `hostname`"
    mgm_nodes=1
    mgm_node_1=`hostname`
fi

if [ -n "$dst_dir" ]
then
    mkdir -p $dst_dir
    if [ ! -d $dst_dir ]
    then
	syndie "Unable to create dst dir: $dst_dir"
    fi
    DST=/tmp/$uniq_id
fi

# --- option verifying done ---

# Find uniq computers
i=1
while [ $i -le $mgm_nodes ]
do
    echo `eval echo "\$"mgm_node_${i}_host` >> /tmp/hosts.$uniq_id
    i=`expr $i + 1`
done

i=1
while [ $i -le $ndb_nodes ]
do
    echo `eval echo "\$"ndb_node_${i}_host` >> /tmp/hosts.$uniq_id
    i=`expr $i + 1`
done

i=1
while [ $i -le $api_nodes ]
do
    echo `eval echo "\$"api_node_${i}_host` >> /tmp/hosts.$uniq_id
    i=`expr $i + 1`
done

sort -u -o /tmp/hosts.$uniq_id /tmp/hosts.$uniq_id

get_computer_id(){
    grep -w -n $1 /tmp/hosts.$uniq_id | cut -d ":" -f 1
}

get_mgm_computer_id(){
    a=`eval echo "\$"mgm_node_${1}_host`
    get_computer_id $a
}

get_ndb_computer_id(){
    a=`eval echo "\$"ndb_node_${1}_host`
    get_computer_id $a
}

get_api_computer_id(){
    a=`eval echo "\$"api_node_${1}_host`
    get_computer_id $a
}

# -- Write config files --

mgm_port=$baseport

(
    i=1
    #echo "COMPUTERS"
    cat /tmp/hosts.$uniq_id | while read host
    do
	echo "[COMPUTER]"
	echo "Id: $i"
	echo "ByteOrder: Big"
	echo "HostName: $host"
	echo
	i=`expr $i + 1`
    done

    node_id=1
    echo

    # Mgm process 
    echo
    echo "[MGM]"
    echo "Id: $node_id"
    echo "ExecuteOnComputer: `get_mgm_computer_id 1`"
    echo "PortNumber: $mgm_port"
    node_id=`expr $node_id + 1`

    # Ndb processes
    i=1
    ndb_nodes=`trim $ndb_nodes`
    while [ $i -le $ndb_nodes ]
    do
	echo
	echo "[DB]"
	echo "Id: $node_id"
	echo "ExecuteOnComputer: `get_ndb_computer_id $i`"
	echo "FileSystemPath: $basedir/run/node-${node_id}-fs"
	i=`expr $i + 1`
	node_id=`expr $node_id + 1`
    done

    # API processes
    i=1
    while [ $i -le $api_nodes ]
    do
	echo
	echo "[API]"
	echo "Id: $node_id"
	echo "ExecuteOnComputer: `get_api_computer_id $i`"
	i=`expr $i + 1`
	node_id=`expr $node_id + 1`
    done

    # Connections
    current_port=`expr $mgm_port + 1`
    echo

    # Connect Mgm with all ndb-nodes
    i=1
    while [ $i -le $ndb_nodes ]
    do
	echo
	echo "[TCP]"
	echo "NodeId1: 1"
	echo "NodeId2: `expr $i + 1`"
	echo "PortNumber: $current_port"
	i=`expr $i + 1`
	current_port=`expr $current_port + 1`
    done

    # Connect All ndb processes with all ndb processes
    i=1
    while [ $i -le $ndb_nodes ]
    do
	j=`expr $i + 1`
	while [ $j -le $ndb_nodes ]
	do
	    echo
	    echo "[TCP]"
	    echo "NodeId1: `expr $i + 1`"
	    echo "NodeId2: `expr $j + 1`"
	    echo "PortNumber: $current_port"
	    j=`expr $j + 1`
	    current_port=`expr $current_port + 1`
	done
	i=`expr $i + 1`
    done

    # Connect all ndb-nodes with all api nodes
    i=1
    while [ $i -le $ndb_nodes ]
    do
	j=1
	while [ $j -le $api_nodes ]
	do
	    echo
	    echo "[TCP]"
	    echo "NodeId1: `expr $i + 1`"
	    echo "NodeId2: `expr $j + $ndb_nodes + 1`"
	    echo "PortNumber: $current_port"
	    j=`expr $j + 1`
	    current_port=`expr $current_port + 1`
	done
	i=`expr $i + 1`
    done
    echo
) > $DST

trace "Init config file done"

if [ -z "$dst_dir" ]
then
    cat $DST
    rm -f $DST
    rm -f /tmp/hosts.$uniq_id
    exit 0
fi

###
# Create Ndb.cfg files

# nodeid=2;host=localhost:2200

# Mgm node
mkcfg(){
	mkdir -p $dst_dir/${2}.ndb_${1}
	(
		echo "OwnProcessId $2"
		echo "host://${mgm_node_1_host}:${mgm_port}"
	) > $dst_dir/${2}.ndb_${1}/Ndb.cfg
	if [ $1 = "db" ]
	then
	    mkdir $dst_dir/node-${2}-fs
	fi
}

mkcfg mgm 1 
cat $DST > $dst_dir/1.ndb_mgm/initconfig.txt

trace "Creating Ndb.cfg for ndb nodes"

current_node=2
i=1
while [ $i -le $ndb_nodes ]
do
    mkcfg db ${current_node}
    i=`expr $i + 1`
    current_node=`expr $current_node + 1`
done

trace "Creating Ndb.cfg for api nodes"

i=1
while [ $i -le $api_nodes ]
do
    mkcfg api ${current_node}
    i=`expr $i + 1`
    current_node=`expr $current_node + 1`
done

rm -f $DST
rm -f /tmp/hosts.$uniq_id


exit 0
# vim: set sw=4:
