#!@PERL@

$version="1.02";

use Getopt::Long;

$opt_help=$opt_Information=$opt_skip_use_db=0;
$opt_regexp=$opt_dbregexp=".*";
$opt_start_row=1; $opt_rows=9999999999;

GetOptions("Information","help","regexp=s","start_row=i","rows=i",
	   "dbregexp=s", "skip-use-db")
  || usage();
usage() if ($opt_help || $opt_Information);

$query=$search=$database=$set=""; $eoq=0;
while (<>)
{
  next if (length($query) == 0 && /^\#/); # Skipp comments
  $query.=search($_);
  if ($eoq)
  {
    if ($query =~ /^use /i || $query =~ /^SET / ||
	($query =~ /$opt_regexp/o && $database =~ /$opt_dbregexp/o))
    {
      if ($opt_skip_use_db && $query =~ /^use /i)
      {
	$query="";
	next;
      }
      if ($opt_start_row <= 1)
      {
	if ($database)
	{
	  print $database, $set;
	  $database=$set="";
	}
	print $query;
	last if (--$opt_rows == 0);
      }
      else
      {
	$opt_start_row--;
	if ($query =~ /^use /)
	{
	  $database=$query;
	  $set="";
	}
	elsif ($query =~ /^SET/)
	{
	  $set=$query;
	}
	else
	{
	  $set="";
	}
      }
    }
    $query=""; $search=""; $eoq=0;
  }
}

exit 0;

sub search
{
  my($row)=shift;
  my($i);

  for ($i=0 ; $i < length($row) ; $i++)
  {
    if (length($search))
    {
      if (length($search) > 1)
      {				# Comment
	next if (substr($row,$i,length($search)) ne $search);
	$i+=length($search)-1;
	$search="";
      }
      elsif (substr($row,$i,1) eq '\\') # Escaped char in string
      {
	$i++;
      }
      elsif (substr($row,$i,1) eq $search)
      {
	if (substr($row,$i+1,1) eq $search)	# Double " or '
	{
	  $i++;
	}
	else
	{
	  $search="";
	}
      }
      next;	
    }
    if (substr($row,$i,2) eq '/*')	# Comment
    {
      $search="*/";
      $i++;
    }
    elsif (substr($row,$i,1) eq "'" || substr($row,$i,1) eq '"')
    {
      $search=substr($row,$i,1);
     }
  }
  $eoq=1 if (!length($search) && $row =~ /;\s*$/);
  return $row;
}


sub usage
{
    print <<EOF;
$0  Ver $version

TCX Datakonsult AB, by Monty.
This software comes with NO WARRANTY: see the file PUBLIC for details.

Prints all SQL queries that matches a regexp or contains a 'use
database' or 'set ..' command to stdout.  A SQL query may contain
newlines.  This is useful to find things in a MySQL update log.

$0 takes the following options:

--help or --Information
  Shows this help

--regexp=#
  Print queries that matches this.

--start_row=#
  Start output from this row (first row = 1)

--skip-use-db
  Don\'t include \'use database\' commands in the output.

--rows=#
  Quit after this many rows.

Example:

$0 --regexp "problem_table" < update.log

$0 --regexp "problem_table" update-log.1 update-log.2
EOF
  exit(0);
}
