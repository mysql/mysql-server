#!/usr/bin/perl
# Copyright (c) 2000, 2001, 2003, 2006 MySQL AB, 2009 Sun Microsystems, Inc.
# Use is subject to license terms.
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Library General Public
# License as published by the Free Software Foundation; version 2
# of the License.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Library General Public License for more details.
#
# You should have received a copy of the GNU Library General Public
# License along with this library; if not, write to the Free
# Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston,
# MA 02110-1301, USA
#
# a little program to generate a table of results
# just read all the RUN-*.log files and format them nicely
# Made by Luuk de Boer
# Patched by Monty

use Getopt::Long;

$opt_server="mysql";
$opt_dir="output";
$opt_machine=$opt_cmp="";
$opt_relative=$opt_same_server=$opt_help=$opt_Information=$opt_skip_count=$opt_no_bars=$opt_verbose=0;

GetOptions("Information","help","server=s","cmp=s","machine=s","relative","same-server","dir=s","skip-count","no-bars","html","verbose") || usage();

usage() if ($opt_help || $opt_Information);

$opt_cmp=lc(join(",",sort(split(',',$opt_cmp))));

if ($opt_same_server)
{
  $files="$opt_dir/RUN-$opt_server*$opt_machine";
}
else
{
  $files="$opt_dir/RUN-*$opt_machine";
}
$files.= "-cmp-$opt_cmp" if (length($opt_cmp));

#
# Go trough all RUN files and gather statistics.
#

if ($#ARGV == -1)
{
  @ARGV=glob($files);
  $automatic_files=1;
}

foreach (@ARGV)
{
  next if (!$opt_cmp && /-cmp-/ && $automatic_files || defined($found{$_}));
  $prog=$filename = $_;
  $found{$_}=1;			# Remove dupplicates
  /RUN-(.*)$/;
  $tot{$prog}{'version'}=$1;
  push(@key_order,$prog);
  $next = 0;
  open(TMP, "<$filename") || die "Can't open $filename: $!\n";
  while (<TMP>)
  {
    chomp;
    if ($next == 0) {
      if (/Server version:\s+(\S+.*)/i)
      {
	$tot{$prog}{'server'} = $1;
      }
      elsif (/Arguments:\s+(.+)/i)
      {
	$arguments= $1;
	# Remove some standard, not informative arguments
	$arguments =~ s/--force|--log|--use-old\S*|--server=\S+|--cmp=\S+|--user=\S+|--pass=\S+|--machine=\S+|--dir=\S+//g;
	if (($tmp=index($arguments,"--comment")) >= 0)
	{
	  if (($end=index($arguments,$tmp+2,"--")) >= 0)
	  {
	    substr($arguments,$tmp,($end-$tmp))="";
	  }
	  else
	  {
	    $arguments=substr($arguments,0,$tmp);
	  }
	}
	$arguments =~ s/\s+/ /g;
	$tot{$prog}{'arguments'}=$arguments;
      }
      elsif (/Comments:\s+(.+)/i) {
	$tot{$prog}{'comments'} = $1;
      } elsif (/^(\S+):.*(estimated\s|)total\stime:\s+([\d.]+)\s+(wallclock\s|)secs/i)
      {
	$tmp = $1; $tmp =~ s/://;
	$tot{$prog}{$tmp} = [ $3, (length($2) ? "+" : "")];
	$op1{$tmp} = $tmp;
      } elsif (/Totals per operation:/i) {
	$next = 1;
	next;
      }
    }
    elsif ($next == 1)
    {
      if (/^(\S+)\s+([\d.]+)\s+([\d.]+)\s+([\d.]+)\s+([\d.]+)\s+([\d.]+)\s*([+|?])*/)
      {
	$tot1{$prog}{$1} = [$2,$6,$7];
	$op{$1} = $1;
      }
    }
  }
}

if (!%op)
{
  print "Didn't find any files matching: '$files'\n";
  print "Use the --cmp=server,server option to compare benchmarks\n";
  exit 1;
}


# everything is loaded ...
# now we have to create a fancy output :-)

# I prefer to redirect scripts instead to force it to file ; Monty
#
# open(RES, ">$resultfile") || die "Can't write to $resultfile: $!\n";
# select(RES)
#

if ($opt_html) {
  html_output();
} else {
  ascii_output();
}
exit 0;

#
# some output + format functions;
#

sub ascii_output {
  print <<EOF;
This is the result file of the different benchmark tests.

The number in () after each tests shows how many SQL commands the particular
test did.  As one test may have many different parameters this gives only
a rough picture of what was done.  Check the source for more information :)

Keep in mind that one can\'t compare benchmarks run with different --cmp
options. The --cmp options sets the all limits according to the worst
limit for all server in the benchmark.

Numbers marked with '+' are estimated according to previous runs because
the query took longer than a given time-limit to finish. The estimation
shouldn\'t be far from the real result thought.

Numbers marked with '?' contains results that gave wrong result. This can only
be used as an indication of how long it took for the server to produce a wrong
result :)

Numbers marked with '*' are tests that was run a different number times
than the test in the first column.  The reason for this is normally that the
marked test was run with different options that affects the number of tests
or that the test was from another version of the MySQL benchmarks.

Hope this will give you some idea how each db is performing at what thing ....
Hope you like it .... Luuk & Monty (1997)

EOF

  if ($opt_relative)
  {
    print "Column 1 is in seconds. All other columns are presented relative\n";
    print "to this. 1.00 is the same, bigger numbers indicates slower\n\n";
  }

  if ($opt_verbose)
  {
    print "The test values is printed in the format 'time:number of tests'\n";
  }

  if (length($opt_cmp))
  {
    print "The test was run with limits from: $opt_cmp\n\n";
  }
  print "The result logs which where found and the options:\n";
  $bar= $opt_no_bars ? " " : "|";

  # Move $opt_server first in array if not filename on command line
  if ($automatic_files)
  {
    @key_order=sort {$a cmp $b} keys %tot;
    for ($i=0; $i <= $#key_order; $i++)
    {
      if ($tot{$key_order[$i]}{'version'} =~ /^$opt_server-/)
      {
	unshift(@key_order,$key_order[$i]);
	splice(@key_order,$i+1,1);
	last;
      }
    }
  }
  # Print header

  $column_count=0;
  foreach $key (@key_order)
  {
    $tmp=$tmp=$tot{$key}{'version'};
    $tmp =~ s/-cmp-$opt_cmp// if (length($opt_cmp));
    $column_count++;
    printf "%2d %-40.40s: %s %s\n", $column_count, $tmp,
    $tot{$key}{'server'}, $tot{$key}{'arguments'};
    print "  $tot{$key}{'comments'}\n"
      if ($tot{$key}{'comments'} =~ /\w+/);
  }

  print "\n";

  $namewidth=($opt_skip_count && !$opt_verbose) ? 29 : 36;
  $colwidth= $opt_relative ? 10 : 7;
  $count_width=7;
  $colwidth+=$count_width if ($opt_verbose);

  print_sep("=");
  printf "%-$namewidth.${namewidth}s${bar}", "Operation";
  $count = 1;
  foreach $key (@key_order)
  {
    printf "%${colwidth}d${bar}", $count;
    $count++;
  }
  printf "\n%-$namewidth.${namewidth}s${bar}", "";
  foreach $key (@key_order)
  {
    $ver=$tot{$key}{'version'};
    $ver =~ s/-[a-zA-Z0-9_\.]+-cmp-$opt-cmp$//;
    printf "%${colwidth}.${colwidth}s${bar}", $ver;
  }
  print "\n";
  print_sep("-");
  print_string($opt_relative ? "Relative results per test (First column is in seconds):" : "Results per test in seconds:");
  print_sep("-");

  foreach $key (sort {$a cmp $b} keys %op1)
  {
    printf "%-$namewidth.${namewidth}s${bar}", $key;
    $first=undef();
    foreach $server (@key_order)
    {
      print_value($first,$tot{$server}{$key}->[0],undef(),$tot{$server}{$key}->[1]);
      $first=$tot{$server}{$key}->[0] if (!defined($first));
    }
    print "\n";
  }

  print_sep("-");
  print_string("The results per operation:");
  print_sep("-");

  foreach $key (sort {$a cmp $b} keys %op)
  {
    next if ($key =~ /TOTALS/i);
    $tmp=$key;
    $count=$tot1{$key_order[0]}{$key}->[1];
    $tmp.= " (" . $count .  ")" if (!$skip_count);
    printf "%-$namewidth.${namewidth}s${bar}", $tmp;
    $first=undef();
    foreach $server (@key_order)
    {
      $tmp= $count != $tot1{$server}{$key}->[1] ? "*" : "";
      print_value($first,$tot1{$server}{$key}->[0],$tot1{$server}{$key}->[1],
		  $tot1{$server}{$key}->[2] . $tmp);
      $first=$tot1{$server}{$key}->[0] if (!defined($first));
    }
    print "\n";
  }

  print_sep("-");
  $key="TOTALS";
  printf "%-$namewidth.${namewidth}s${bar}", $key;
  $first=undef();
  foreach $server (@key_order)
  {
    print_value($first,$tot1{$server}{$key}->[0],undef(),
		$tot1{$server}{$key}->[2]);
    $first=$tot1{$server}{$key}->[0] if (!defined($first));
  }
  print "\n";
  print_sep("=");
}


sub html_output
{
  my $template="template.html";
  my $title="MySQL | | Information | Benchmarks | Compare with $opt_cmp";
  my $image="info.gif";
  $bar="";

  open(TEMPLATE, $template) || die;
  while (<TEMPLATE>)
  {
    if (/<center>/)
    {
      print $_;
      print "<!---- This is AUTOMATICALLY Generated. Do not edit here! ---->\n";
    }
    elsif (/TITLE:SUBTITLE/)
    {
      s|TITLE:SUBTITLE|$title|;
      print $_;
    }
    elsif (/TITLE:COMPARE/)
    {
      s|TITLE:COMPARE|$opt_cmp|;
      print $_;
    }
    elsif (/ subchapter name /)
    {
      # Nothing here for now
      print $_;
    }
    elsif (/ text of chapter /)
    {
      print $_;
      print_html_body();
    }
    else
    {
      print $_;
    }
  }
  close(TEMPLATE);
}


sub print_html_body
{
  my ($title,$count,$key);
  print <<EOF;
<center>
<font size=+4><b>MySQL Benchmark Results</b></font><br>
<font size=+1><b>Compare with $opt_cmp</b></font><p><p>
</center>
This is the result file of the different benchmark tests.
<p>

The number in () after each tests shows how many SQL commands the particular
test did.  As one test may have many different parameters this gives only
a rough picture of what was done.  Check the source for more information.
<p>
Keep in mind that one can\'t compare benchmarks run with different --cmp
options. The --cmp options sets the all limits according to the worst
limit for all server in the benchmark.
<p>
Numbers marked with '+' are estimated according to previous runs because
the query took longer than a given time-limit to finish. The estimation
shouldn\'t be far from the real result thought.
<p>
Numbers marked with '?' contains results that gave wrong result. This can only
be used as an indication of how long it took for the server to produce a wrong
result :)
<p>
Hope this will give you some idea how each db is performing at what thing ....
<br>
Hope you like it .... Luuk & Monty (1997)
<p><p>
EOF

  if ($opt_relative)
  {
    print "Column 1 is in seconds. All other columns are presented relative<br>\n";
    print "to this. 1.00 is the same, bigger numbers indicates slower<p>\n\n";
  }

  if (length($opt_cmp))
  {
    print "The test was run with limits from: $opt_cmp\n\n";
  }
  print "The result logs which where found and the options:<br>\n";

  # Move $opt_server first in array
  if ($automatic_files)
  {
    @key_order=sort {$a cmp $b} keys %tot;
    for ($i=0; $i <= $#key_order; $i++)
    {
      if ($tot{$key_order[$i]}{'version'} =~ /^$opt_server-/)
      {
	unshift(@key_order,$key_order[$i]);
	splice(@key_order,$i+1,1);
	last;
      }
    }
  }
  # Print header
  print "<p><center><table border=1 width=100%>\n";
  $column_count=0;
  foreach $key (@key_order)
  {
    $tmp=$tot{$key}{'version'};
    $tmp =~ s/-cmp-$opt_cmp// if (length($opt_cmp));
    $column_count++;
#    printf "<tr><td>%2d<td>%-36.36s<td>%s %s</tr>\n", $column_count, $tmp,
    printf "<tr><td>%2d</td><td>%s</td><td>%s %s</td></tr>\n",
    $column_count, $tmp, $tot{$key}{'server'}, $tot{$key}{'arguments'};
    print "<tr><td colspan=3>$tot{$key}{'comments'}</td></tr>\n"
      if ($tot{$key}{'comments'} =~ /\w+/);
  }

  print "</table></center><p><center><table border=1 width=100%>\n";

  $namewidth=$opt_skip_count ? 22 :29;
  $colwidth= $opt_relative ? 10 : 7;
  $count_width=7;

  printf "<tr><td><b>%s</b></td>\n", "Operation";
  $count = 1;
  foreach $key (@key_order)
  {
    $ver=$tot{$key}{'version'};
    printf "<td align=center><b>%d", $count;
    printf "<br>%${colwidth}.${colwidth}s</b></td>\n", substr($ver,0,index($ver,"-"));
    $count++;
  }
  print "</tr>\n";
  $title = $opt_relative ? "Relative results per test (First column is in seconds):" : "Results per test in seconds:";
  printf "<tr><td colspan=%d><b>%s</b></td></tr>\n", $count, $title;

  foreach $key (sort {$a cmp $b} keys %op1)
  {
    if (!$opt_html)
    {
      printf "<tr><td>%-$namewidth.${namewidth}s</td>", $key;
    }
    else
    {
      print "<tr><td>$key</td>";
    }
    $first=undef();
    foreach $server (@key_order)
    {
      print_value($first,$tot{$server}{$key}->[0],undef(),
		  $tot{$server}{$key}->[1]);
      $first=$tot{$server}{$key}->[0] if (!defined($first));
    }
    print "</tr>\n";
  }

  $title = "The results per operation:";
  printf "<tr><td colspan=%d><b>%s</b></td></tr>\n", $count, $title;

  foreach $key (sort {$a cmp $b} keys %op)
  {
    next if ($key =~ /TOTALS/i);
    $tmp=$key;
    $tmp.= " (" . $tot1{$key_order[0]}{$key}->[1] . ")" if (!$skip_count);
    if (!$opt_html)
    {
      printf "<tr><td>%-$namewidth.${namewidth}s</td>", $tmp;
    }
    else
    {
      print "<tr><td>$tmp</td>";
    }
    $first=undef();
    foreach $server (@key_order)
    {
      print_value($first,$tot1{$server}{$key}->[0],
		  $tot1{$server}{$key}->[1],
		  $tot1{$server}{$key}->[2]);
      $first=$tot1{$server}{$key}->[0] if (!defined($first));
    }
    print "</tr>\n";
  }

  $key="TOTALS";
  printf "<tr><td><b>%-$namewidth.${namewidth}s</b></td>", $key;
  $first=undef();
  foreach $server (@key_order)
  {
    print_value($first,$tot1{$server}{$key}->[0],undef(),
		$tot1{$server}{$key}->[2]);
    $first=$tot1{$server}{$key}->[0] if (!defined($first));
  }
  print "</tr>\n</table>\n";
}


sub print_sep
{
  my ($sep)=@_;
  print $sep x ($namewidth + (($colwidth+1) * $column_count)+1),"\n";
}


sub print_value
{
  my ($first,$value,$count,$flags)=@_;
  my ($tmp,$width);

  if (defined($value))
  {
    if (!defined($first) || !$opt_relative)
    {
      $tmp=sprintf("%.2f",$value);
    }
    else
    {
      $first=1 if ($first == 0); # Assume that it took one second instead of 0
      $tmp= sprintf("%.2f",$value/$first);
    }
    if (defined($flags))
    {
      $tmp="+".$tmp if ($flags =~ /\+/);
      $tmp="?".$tmp if ($flags =~ /\?/);
      $tmp="*".$tmp if ($flags =~ /\*/);
    }
  }
  else
  {
    $tmp="";
  }
  $width= ($opt_verbose ? $colwidth - $count_width : $colwidth);
  if (!$opt_html)
  {
    $tmp= " " x ($width-length($tmp)) . $tmp if (length($tmp) < $width);
  }
  if ($opt_verbose)
  {
    if ($count)
    {
      $tmp.= ":" . " " x ($count_width-1-length($count)) . $count;
    }
    else
    {
      $tmp.= " " x ($count_width);
    }
  }

  if (!$opt_html) {
    print $tmp . "${bar}";
  } else {
    print "<td align=right>$tmp</td>";
  }
}


sub print_string
{
  my ($str)=@_;
  if (!$opt_html)
  {
    my ($width);
    $width=$namewidth + ($colwidth+1)*$column_count;
    $str=substr($str,1,$width) if (length($str) > $width);
    print($str," " x ($width - length($str)),"${bar}\n");
  }
  else
  {
    print $str,"\n";
  }
}


sub usage
{
    print <<EOF;
$0  Ver 1.2

This program parses all RUN files from old 'run-all-tests --log' scripts
and makes a nice comparable table.

$0 takes currently the following options:

--help or --Information		
  Shows this help

--cmp=server,server,server (Default $opt_cmp)
Compares all runs that are done with the same --cmp options to run-all-tests.
The most normal options are '--cmp=mysql,pg,solid' and '--cmp ""'

--dir=...  (Default $opt_dir)
From which directory one should get the runs.  All runs made by
run-all-tests --log is saved in the 'output' directory.
In the 'results' directory you may have some example runs from different
databases.

--html
  Print the table in html format.

--machine='full-machine-name' (Default $opt_machine)
Use only runs that match this machine.

--relative
Show all numbers in times of the first server where the time for the
first server is 1.0

--same-server
Compare all runs for --server=....  The --machine is not used in this case
This is nice to compare how the same server runs on different machines.

--server='server name'  (Default $opt_server)
Put this server in the first result column.

--skip-count
Do not write the number of tests after the test-name.

--verbose
Write the number of tests in each column. This is useful when some column
is marked with '*'.
EOF

  exit(0);
}
