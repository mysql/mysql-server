#!/bin/sh
# NAME
#   signallog2html.sh
#
# SYNOPSIS
#   signallog2html.sh [ -b <block_name | ALL> ] [ -s <signal_id> ] -f signal_log_file
#
# DESCRIPTION
#   Creates a signal sequence diagram in HTML format that can be
#   viewed from a web browser.  The HTML file is created from a signal
#   log file and it contains a big table with jpeg files in every
#   table cell. Every row in the table is a signal.  The block_name
#   could be one of the following: CMVMI MISSRA NDBFS NDBCNTR DBACC
#   DBDICT DBLQH DBDIH DBTC DBTUP QMGR ALL. The signal_id is a
#   number. If no block_name or signal_id is given the default
#   block_name "ALL" is used.
#
#
#
# OPTIONS
#
# EXAMPLES
#   
#   
# ENVIRONMENT
#   NDB_PROJ_HOME                Home dir for ndb
#   
# FILES
#   $NDB_PROJ_HOME/lib/funcs.sh  General shell script functions.
#   uniq_blocks.awk              Creates a list of unique blocks 
#                                in the signal_log_file.
#   signallog2list.awk           Creates a list file from the signal_log_file.
#   empty.JPG                    Jpeg file, must exist in the HTML file 
#                                directory for viewing.
#   left_line.JPG
#   line.JPG
#   right_line.JPG
#   self_line.JPG
#
#
# SEE ALSO
#   
# DIAGNOSTICTS
#   
# VERSION   
#   1.0
#   
# DATE
# 011029
#
# AUTHOR
#   Jan Markborg
#

progname=`basename $0`
synopsis="signallog2html.sh [ -b <block_name | ALL> ] [ -s <signal_id> ] -f signal_log_file"
block_name=""
signal_id=""
verbose=yes
signal_log_file=""

: ${NDB_PROJ_HOME:?}		 # If undefined, exit with error message

: ${NDB_LOCAL_BUILD_OPTIONS:=--} # If undef, set to --. Keeps getopts happy.
			         # You may have to experiment a bit  
                                 # to get quoting right (if you need it).


. $NDB_PROJ_HOME/lib/funcs.sh    # Load some good stuff

# defaults for options related variables 
#
report_date=`date '+%Y-%m-%d'`

# Option parsing for the the command line.
#

while getopts f:b:s: i 
do
    case $i in
	f)      signal_log_file=$OPTARG;;
	b)      block_name=$OPTARG;;
	s)      signal_id=$OPTARG;;
	\?)	syndie ;;    # print synopsis and exit
    esac
done

# -- Verify
trace "Verifying signal_log_file $signal_log_file"

if [ x$signal_log_file = "x" ]
then
    syndie "Invalid signal_log_file name: $signal_log_file not found"
fi


if [ ! -r $signal_log_file ]
then
    syndie "Invalid signal_log_file name: $signal_log_file not found"
fi



if [ blocknameSET = 1 ]
then

    trace "Verifying block_name"
    case $block_name  in
	CMVMI| MISSRA| NDBFS| NDBCNTR| DBACC| DBDICT| DBLQH| DBDIH| DBTC| DBTUP| QMGR);;
	ALL)    trace "Signals to/from every block will be traced!";;
	*)      syndie "Unknown block name: $block_name";;
    esac
fi

if [ block_name="" -a signal_id="" ]
then
    block_name=ALL
    trace "block_name = $block_name"
fi

trace "Arguments OK"

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

table_header(){
    echo "<th>$*</th>"
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
<td align=left>$*</td>
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

img_column(){
    cat <<EOF
<td><center><$* height=100% width=100%></center></td>
EOF
}

# Check the direction of arrow.
# arrowDirection(){ $columnarray $sendnode$sendblock $recnode$recblock
arrowDirection(){
if [ $2 = $3 ]
then
    arrow=SELF
    return;
else
    for x in $1
    do  
	if [ $x = $2 ]
	then
	    arrow=RIGHT
	    break
	elif [ $x = $3 ]
	then
	    arrow=LEFT  
	    break
	fi
    done   
fi
}

drawImages(){
for x in $columnarray
do
    	case $arrow in
	    SELF)
		if [ $x = $sendnode$sendblock ]
		then	
		    img_column img SRC=\"self_line.JPG\" 
		else
		    img_column img SRC=\"empty.JPG\"
		fi;;    
		
	    RIGHT)
		if [ $x = $recnode$recblock ]
		then
		    img_column img SRC=\"right_line.JPG\"
		    weHavePassedRec=1
		elif [ $x = $sendnode$sendblock ]
		then
		    img_column img SRC=\"empty.JPG\"
		    weHavePassedSen=1
		elif [ $weHavePassedRec = 1 -o $weHavePassedSen = 0 ]
		then
		    img_column img SRC=\"empty.JPG\"
		elif  [ $weHavePassedRec = 0  -a $weHavePassedSen = 1 ]
		then
		    img_column img SRC=\"line.JPG\"
		fi;;
		
	    LEFT)
		if [ $x = $recnode$recblock ]
		then
		    img_column img SRC=\"empty.JPG\"
		    weHaveJustPassedRec=1
		    weHavePassedRec=1
		    continue
		fi
		if [ $x = $sendnode$sendblock -a $weHaveJustPassedRec = 1 ]
		then
		    img_column img SRC=\"left_line.JPG\"
		    weHaveJustPassedRec=0
		    weHavePassedSen=1
		    continue
		fi
		if [ $x = $sendnode$sendblock ]
		then
		    img_column img SRC=\"line.JPG\"
		    weHavePassedSen=1
		    continue
		fi
		if [ $weHaveJustPassedRec = 1 ]
		then
		    img_column img SRC=\"left_line.JPG\"
		    weHaveJustPassedRec=0
		    continue
		fi
		if [ $weHavePassedSen = 1 -o $weHavePassedRec = 0 ]
		then
		    img_column img SRC=\"empty.JPG\"
		    continue
		fi

		if [ $weHavePassedRec = 1 -a $weHavePassedSen = 0 ]
		then
		    img_column img SRC=\"line.JPG\"
		    continue
		
		fi
		column ERROR;;

	    *)
		echo ERROR;;
	esac
done
column $signal
}

### Main
trace "Making HTML file"
(
    header  "Signal sequence diagram $report_date"
    heading 1 "Signal sequence diagram $report_date"

    trace "Making list file"
    #make a signal list file from the signal log file.
    `awk -f /home/ndb/bin/signallog2html.lib/signallog2list.awk SIGNAL_ID=$signal_id BLOCK_ID=$block_name $signal_log_file > $signal_log_file.list`

    COLUMNS=`awk -f /home/ndb/bin/signallog2html.lib/uniq_blocks.awk $signal_log_file.list | wc -w`
   
    table "border=0 cellspacing=0 cellpadding=0 cols=`expr $COLUMNS + 1`"

    columnarray=`awk -f /home/ndb/bin/signallog2html.lib/uniq_blocks.awk $signal_log_file.list`
    
    row
    column #make an empty first column!
    for col in $columnarray
    do
    table_header $col
    done

    grep "" $signal_log_file.list | \
    while read direction sendnode sendblock recnode recblock signal sigid recsigid delay
    do
 	if [ $direction = "R" ]
	then
	    row 
	    weHavePassedRec=0
	    weHavePassedSen=0
	    weHaveJustPassedRec=0
	    arrow=""
	    
	    # calculate the direction of the arrow.
	    arrowDirection "$columnarray" "$sendnode$sendblock" "$recnode$recblock"
	    
	    # Draw the arrow images.
	    drawImages
	    end_row
 	fi
    done
    end_table

    footer
)  > $signal_log_file.html

exit 0
