#!/bin/sh
# NAME
#   make-html-reports.sh
#
# SYNOPSIS
#   make-html-reports.sh [-q] [ -R <YYYY-MM-DD> ] [ -s <src dir> ] [ -d <dst dir> ] [ -c <conf dir> ]
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
synopsis="make-html-reports.sh [ -R <YYYY-MM-DD> ] [ -s <src dir> ] [ -d <dst dir> ] [ -c <conf dir> ]"

: ${NDB_PROJ_HOME:?}		 # If undefined, exit with error message

: ${NDB_LOCAL_BUILD_OPTIONS:=--} # If undef, set to --. Keeps getopts happy.
			         # You may have to experiment a bit  
                                 # to get quoting right (if you need it).


. $NDB_PROJ_HOME/lib/funcs.sh    # Load some good stuff

# defaults for options related variables 
#


src_dir=`pwd`
dst_dir=`pwd`
conf_dir=`pwd`
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

    while getopts q:s:R:d:c: i $optstring  # optstring empty => no arg => cmd line
    do
	case $i in

	q)	verbose="";;		# echo important things
	d)      dst_dir=$OPTARG;;       # Destination directory
	s)      src_dir=$OPTARG;;       # Destination directory
	c)      conf_dir=$OPTARG;;       #
	R)      report_date=$OPTARG;;   #
	\?)	syndie $env_opterr;;    # print synopsis and exit

	esac
    done

    [ -n "$optstring" ]  &&  OPTIND=1 	# Reset for round 2, cmdline options

    env_opterr= 			# Round 2 should not use the value

done
shift `expr $OPTIND - 1`

src_dir=`abspath $src_dir`
dst_dir=`abspath $dst_dir`
conf_dir=`abspath $conf_dir`

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

# --- option parsing done ---

# -- Verify
trace "Verifying arguments"
summary_file=$src_dir/reports/summary.$report_date

if [ ! -r $summary_file ]
then
    syndie "Invalid src directory or report date: $summary_file not found"
fi

if [ ! -d $conf_dir/configurations ]
then
    syndie "Invalid src directory: $conf_dir/configurations not found"
fi

if [ ! -d $conf_dir/testcases ]
then
    syndie "Invalid src directory: $conf_dir/testcases not found"
fi

if [ ! -d $dst_dir ]
then
    syndie "Invalid dst dir..."
fi

# --- option verifying done ---

trace "src_dir:     $src_dir"
trace "dst_dir:     $dst_dir"
trace "conf_dir:    $conf_dir"
trace "report date: $report_date"

###
config_spec(){
	cat <<EOF
<a href=#$1>$1</a>
EOF
}

config_spec_include(){
  # Print the $1 file to the file we are generating
    cat <<EOF 
<a name=$1><pre>
EOF
    if [ -r $conf_dir/configurations/$1 ]
    then
	cat -E $conf_dir/configurations/$1 | sed 's/\$/<BR>/g'  
    else 
	cat <<EOF
 Config spec $1 not found
EOF
    fi
cat <<EOF
</pre></a>
EOF
}

time_spec(){
    # $1 - secs
    _ts_tmp=$1
    
    _ts_s=`expr $_ts_tmp % 60`
    _ts_tmp=`expr $_ts_tmp / 60`
    
    _ts_m=`expr $_ts_tmp % 60`
    _ts_tmp=`expr $_ts_tmp / 60`

    _ts_h=$_ts_tmp

    if [ $_ts_h -gt 0 ]
    then
	ret="${_ts_h}h"
    fi

    [ $_ts_m -gt 0 ] || [ $_ts_h -gt 0 ] && ret="$ret${_ts_m}m"

    ret="$ret${_ts_s}s"
    echo $ret
}

log_spec(){
    _ff_=$src_dir/log/$report_date/$1.$2/test.$3.out
    if [ -r $_ff_ ] && [ -s $_ff_ ] 
    then
	_f2_=$dst_dir/log.$report_date.$1.$2.$3.out.gz
        if [ -r $_f2_ ]
	then 
          rm $_f2_
        fi
	cp $_ff_ $dst_dir/log.$report_date.$1.$2.$3.out
	gzip $dst_dir/log.$report_date.$1.$2.$3.out
	rm -f $dst_dir/log.$report_date.$1.$2.$3.out
	echo "<a href=log.$report_date.$1.$2.$3.out.gz>Log file</a>"
    else
	echo "-"
    fi
}

err_spec(){
    _ff_=$src_dir/log/$report_date/$1.$2/test.$3.err.tar
    if [ -r $_ff_ ] && [ -s $_ff_ ] 
    then
	cp $_ff_ $dst_dir/err.$report_date.$1.$2.$3.err.tar
	gzip $dst_dir/err.$report_date.$1.$2.$3.err.tar
	rm -f $dst_dir/err.$report_date.$1.$2.$3.err.tar
	echo "<a href=err.$report_date.$1.$2.$3.err.tar.gz>Error tarball</a>"
    else
	echo "-"
    fi
}

command_spec(){
	echo $* | sed 's/;/<BR>/g'
}

### Main

html_summary_file=$dst_dir/summary.$report_date.html

trace "Creating summary"
(
    eval `grep "TOTAL" $summary_file | awk -F";" '{ printf("test_file=\"%s\"; elapsed=\"%s\"; started=\"%s\"; stopped=\"%s\"", $2, $3, $4, $5); }'`

    header  "Autotest summary $report_date"
    heading 1 "Autotest summary $report_date"
    table
    row ; column `bold test file: `; column $test_file ; end_row
    row ; column `bold Started:` ; column "$started "; end_row
    row ; column `bold Stopped:` ; column "$stopped "; end_row
    row ; column `bold Elapsed:` ; column "`time_spec $elapsed secs`" ; end_row
    end_table
    hr

    table "border=1"
    row 
    c_column `bold Report`
    c_column `bold Tag` 
    c_column `bold Version`
    c_column `bold Distr-Config`
    c_column `bold Db-Config`
    c_column `bold Type`
    c_column `bold Test file`
    c_column `bold Make`
    c_column `bold Config`
    c_column `bold Test time`
    c_column `bold Passed`
    c_column `bold Failed`
    end_row

    grep -v "^#" $summary_file | grep -v TOTAL | sed 's/;/ /g' | \
    while read tag version config template type test_file make_res make_time conf_res conf_time test_time passed failed
    do
	row
	if [ -r $src_dir/reports/report.$tag.$version.$config.$template.$type.$test_file.$report_date ]
	then
	    column "<a href=\"report.$tag.$version.$config.$template.$type.$test_file.$report_date.html\">report</a>"
	else
	    column "-"
        fi

	column $tag
	column $version
	column $config
	column $template
	column $type
	column $test_file
	column "$make_res(`time_spec $make_time`)"
	column "$conf_res(`time_spec $conf_time`)"
	c_column "`time_spec $test_time`"
	c_column `bold $passed`
	c_column `bold $failed`
	end_row
    done
    end_table

    footer
) > $html_summary_file

for i in $src_dir/reports/report.*.$report_date
do
    f=`basename $i`
    trace "Creating report: $f"
    eval `echo $f | awk -F"." '{printf("tag=%s;version=%s;config=%s;template=%s;type=%s;test_file=%s", $2, $3, $4, $5, $6, $7);}'`

    (
	header  "Autotest report $report_date"
	heading 1 "Autotest report $report_date"
	table #"border=1"
	row ; column `bold Tag:`; column $tag ; end_row
	row ; column `bold Version:` ; column $version ; end_row
	row ; column `bold Configuration:` ; column `config_spec $config`; end_row
	row ; column `bold Template:` ; column `config_spec $template`; end_row
	row ; column `bold Type:` ; column $type ; end_row
	row ; column `bold Test file:` ; column $test_file; end_row
	end_table
	hr

	table "border=1"
	row 
	c_column `bold Test case`
	c_column `bold Result` 
	c_column `bold Test time`
	c_column `bold Logfile`
	c_column `bold Error tarfile`
	end_row

	grep -v "^#" $i | sed 's/;/ /g' | \
	while read test_no test_res test_time cmd
	do
	    row
	    column "`command_spec $cmd`"
	    case "$test_res" in
		0)
		    column "PASSED";;
		1001)
		    column "API error";;
		1002)
		    column "Max time expired";;
		1003)
		    column "Mgm port busy";;
		*)
		    column "Unknown: $test_res";;
	    esac

	    column "`time_spec $test_time`"

	    column "`log_spec $tag $version $test_no`"
	    column "`err_spec $tag $version $test_no`"
	    end_row
	done
	end_table

        # Last on page we include spec 
        # of used machines and template for config
        # for future reference
        hr
	table "border=1"
	row; column `bold Configuration:` $config; end_row
	row; column `config_spec_include $config`; end_row
        end_table
        hr
        table "border=1"
	row; column `bold Template:` $template; end_row
	row; column `config_spec_include $template`; end_row
	end_table       

	footer
	
    ) > $dst_dir/$f.html
done

# Re creating index
trace "Recreating index"
(
    header "Autotest super-duper index"
    heading 1 "<center>Autotest super-duper index</center>"
    hr
    for i in `ls $dst_dir/summary.*.html | sort -r -n`
    do
	f=`basename $i`
	cat <<EOF
<p><a href=$f>$f</a></p>
EOF
    done
    footer
) > $dst_dir/index.html

exit 0
