#!/bin/sh

# Copyright (c) 2003, 2022, Oracle and/or its affiliates.
# Use is subject to license terms
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2.0,
# as published by the Free Software Foundation.
#
# This program is also distributed with certain software (including
# but not limited to OpenSSL) that is licensed under separate terms,
# as designated in a particular file or component or in included license
# documentation.  The authors of MySQL hereby grant you an additional
# permission to link the program and your derivative works with the
# separately licensed software that they have included with MySQL.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License, version 2.0, for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

# NAME
#   make-index.sh
#
# SYNOPSIS
#   make-index.sh [ -d <dir> ]
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
#   
# AUTHOR
#   Jonas Oreland
#

progname=`basename $0`
synopsis="make-index.sh [ -d <dir> ]"

: ${NDB_PROJ_HOME:?}		 # If undefined, exit with error message

: ${NDB_LOCAL_BUILD_OPTIONS:=--} # If undef, set to --. Keeps getopts happy.
			         # You may have to experiment a bit  
                                 # to get quoting right (if you need it).


. $NDB_PROJ_HOME/lib/funcs.sh    # Load some good stuff

# defaults for options related variables 
#

dst_dir=/home/autotest/html
report_date=`date '+%Y-%m-%d'`
uniq_id=$$.$$
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
#

for optstring in "$options"  ""	     # 1. options variable  2. cmd line
do

    while getopts q:s:R:d: i $optstring  # optstring empty => no arg => cmd line
    do
	case $i in

	q)	verbose="";;		# echo important things
	d)      dst_dir=$OPTARG;;       # Destination directory
	\?)	syndie $env_opterr;;    # print synopsis and exit

	esac
    done

    [ -n "$optstring" ]  &&  OPTIND=1 	# Reset for round 2, cmdline options

    env_opterr= 			# Round 2 should not use the value

done
shift `expr $OPTIND - 1`

dst_dir=`abspath $dst_dir`

###
#
# General html functions
header(){
    cat <<EOF
<html><head><title>$*</title></head>
<body>
EOF
}

footer(){
    cat <<EOF
</body></html>
EOF
}

heading(){
    h=$1; shift
    cat <<EOF
<h$h>$*</h$h>
EOF
}

table(){
    echo "<table $*>"
}

end_table(){
    echo "</table>"
}

row(){
    echo "<tr>"
}

end_row(){
    echo "</tr>"
}

c_column(){
    cat <<EOF
<td valign=center align=center>$*</td>
EOF
}

bold(){
    cat <<EOF
<b>$*</b>
EOF
}
column(){
    cat <<EOF
<td valign=center align=left>$*</td>
EOF
}

para(){
    cat <<EOF
<p></p>
EOF
}

hr(){
    cat <<EOF
<hr>
EOF
}

inc_summary() {
	grep -v 'html>' $2 | grep -v body |  sed 's/href="/href="'$1'\//g'
}

# --- option parsing done ---



# -- Verify
trace "Verifying arguments"

# --- option verifying done ---

### Main

# Re creating index
trace "Creating index"
(
    header "Autotest super-duper index"
    heading 1 "<center>Autotest super-duper index</center>"
    cat -E README.autotest | sed 's/\$/<BR>/g'
    echo "<br>" 
    echo "Current <a href="crontab.current">crontab</a> installed on mc01 running [" `uname -a` "]"
    hr

    dirs=`find $dst_dir -name 'summary.*.html' -type f -maxdepth 2 -exec dirname {} \; | sort -u`

    dates=`find $dst_dir -name 'summary.*.html' -type f -maxdepth 2 -exec basename {} \; | sed 's/summary\.\(.*\)\.html/\1/g' | sort -u | sort -r`

    echo "<p align=center>"

#inline 5 latest reports
    r_count=5
    for d in $dates
    do
	for o in $dirs
	do
            o=`basename $o`
            if [ -r $dst_dir/$o/summary.$d.html ]
            then
                inc_summary $o $dst_dir/$o/summary.$d.html
		hr

		r_count=`expr $r_count - 1`
		if [ $r_count -eq 0 ]
		then
			break 2
		fi
            fi
        done
    done
	
    table "border=1"
    row
    for i in $dirs
    do
	i=`basename $i`
	column `bold $i`
    done
    end_row


    for d in $dates
    do
	row
	for o in $dirs
	do
	    o=`basename $o`
	    if [ -r $dst_dir/$o/summary.$d.html ]
	    then
		column "<a href=$o/summary.$d.html>$d</a>"
	    else
		column ""
	    fi
	done
	end_row
    done
    end_table
    footer
) > $dst_dir/index.html

exit 0
