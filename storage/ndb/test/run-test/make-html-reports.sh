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

src_dir=$1
run=$2
date=$3
src_file=$src_dir/report.txt

if [ ! -f $src_dir/report.txt ]
then
	echo "$src_dir/report.txt is missing"
	exit 1
fi

###
#
# General html functions
trim(){
	echo $*
}

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

# -- Verify
time_spec(){
    # $1 - secs
    _ts_tmp=$1
    
    _ts_s=`expr $_ts_tmp % 60`
    _ts_tmp=`expr $_ts_tmp / 60`
    
    _ts_m=`expr $_ts_tmp % 60`
    if [ $_ts_tmp -ge 60 ]
    then
	_ts_tmp=`expr $_ts_tmp / 60`
    else
	_ts_tmp=0
    fi

    a=3
    _ts_h=$_ts_tmp

    if [ $_ts_h -gt 0 ]
    then
	ret="${_ts_h}h"
    fi

    [ $_ts_m -gt 0 ] || [ $_ts_h -gt 0 ] && ret="$ret${_ts_m}m"

    ret="$ret${_ts_s}s"
    echo $ret
}

### Main

report_file=$src_dir/report.html
summary_file=$src_dir/summary.html

passed=0
failed=0
total=0

pass(){
	passed=`expr $passed + 1`
}

fail(){
	failed=`expr $failed + 1`
}

(
	header Report $run $date
	table "border=1"
	row
	column `bold Test case`
	column `bold Result`
	column `bold Elapsed`
	column `bold Log`
	end_row
) > $report_file

cat $src_file | while read line 
do
    	eval `echo $line | awk -F";" '{ printf("prg=\"%s\"; no=\"%s\"; res=\"%s\"; time=\"%s\"", $1, $2, $3, $4); }'`

	prg=`trim $prg`
	no=`trim $no`
	res=`trim $res`
	time=`trim $time`
	res_dir="<a href=\"result.$no/\">log</a>"

	ts=`time_spec $time`
	res_txt=""
	case $res in
	0) pass; res_txt="PASSED";;
	*) fail; res_txt="FAILED";;
	esac

	if [ ! -d "$src_dir/result.$no" ]; then res_dir="&nbsp;"; fi

	total=`expr $total + $time`

	(
		row 
		column $prg
		column $res_txt
		column $ts 
		column $res_dir
		end_row
	) >> $report_file

	(
        	row
        	column $run
        	column $date
        	column $passed
        	column $failed
        	column `time_spec $total`
        	column "<a href=\"result-$run/$date/report.html\">report</a>"
        	column "<a href=\"result-$run/$date/log.txt\">log.txt</a>"
        	end_row
	) > $summary_file
done

(
	end_table
	footer
) >> $report_file

exit 0
