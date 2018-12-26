# ==== Purpose ====
#
# Grep a file for a pattern, produce a single string out of the
# matching lines, and assert that the string matches a given regular
# expression.
#
# ==== Usage ====
#
# --let $assert_text= TEXT
# --let $assert_file= FILE
# --let $assert_select= REGEX
# [--let $assert_match= REGEX | --let $assert_count= NUMBER]
# [--let $assert_only_after= REGEX]
# --source include/assert_grep.inc
#
# Parameters:
#
#   $assert_text
#     Text that describes what is being checked. This text is written to
#     the query log so it should not contain non-deterministic elements.
#
#   $assert_file
#     File to search.
#
#   $assert_select
#     All lines matching this text will be checked.
#
#   $assert_match
#     The script will find all lines that match $assert_select,
#     concatenate them to a long string, and assert that it matches
#     $assert_match.
#
#   $assert_count
#     Instead of asserting that the selected lines match
#     $assert_match, assert that there were exactly $assert_count
#     matching lines.
#
#   $assert_only_after
#     Reset all the lines matched and the counter when finding this pattern.
#     It is useful for searching things in the mysqld.err log file just
#     after the last server restart for example (discarding the log content
#     of previous server executions).


if (!$assert_text)
{
  --die !!!ERROR IN TEST: you must set $assert_text
}
if (!$assert_file)
{
  --die !!!ERROR IN TEST: you must set $assert_file
}
if (!$assert_select)
{
  --die !!!ERROR IN TEST: you must set $assert_select
}
if ($assert_match == '')
{
  if ($assert_count == '')
  {
    --die !!!ERROR IN TEST: you must set either $assert_match or $assert_count
  }
}
if ($assert_match != '')
{
  if ($assert_count != '')
  {
    --echo assert_text='$assert_text' assert_count='$assert_count'
    --die !!!ERROR IN TEST: you must set only one of $assert_match or $assert_count
  }
}


--let $include_filename= assert_grep.inc [$assert_text]
--source include/begin_include_file.inc


--let _AG_ASSERT_TEXT= $assert_text
--let _AG_ASSERT_FILE= $assert_file
--let _AG_ASSERT_SELECT= $assert_select
--let _AG_ASSERT_MATCH= $assert_match
--let _AG_ASSERT_COUNT= $assert_count
--let _AG_OUT= `SELECT CONCAT('$MYSQLTEST_VARDIR/tmp/_ag_', UUID())`
--let _AG_ASSERT_ONLY_AFTER= $assert_only_after


--perl
  use strict;
  use warnings;
  my $file= $ENV{'_AG_ASSERT_FILE'};
  my $assert_select= $ENV{'_AG_ASSERT_SELECT'};
  my $assert_match= $ENV{'_AG_ASSERT_MATCH'};
  my $assert_count= $ENV{'_AG_ASSERT_COUNT'};
  my $assert_only_after= $ENV{'_AG_ASSERT_ONLY_AFTER'};
  my $out= $ENV{'_AG_OUT'};

  if (!defined($assert_count)) {
    $assert_count = '';
  }

  my $result= '';
  my $count= 0;
  open(FILE, "$file") or die("Error $? opening $file: $!\n");
  while (<FILE>) {
    my $line = $_;
    if ($assert_only_after && $line =~ /$assert_only_after/) {
      $result = "";
      $count = 0;
    }
    if ($line =~ /$assert_select/) {
      if ($assert_count ne '') {
        $count++;
      }
      else {
        $result .= $line;
      }
    }
  }
  close(FILE) or die("Error $? closing $file: $!");
  open OUT, "> $out" or die("Error $? opening $out: $!");
  if ($assert_count ne '' && ($count != $assert_count)) {
    print OUT ($count) or die("Error $? writing $out: $!");
  }
  elsif ($assert_count eq '' && $result !~ /$assert_match/) {
    print OUT ($result) or die("Error $? writing $out: $!");
  }
  else {
    print OUT ("assert_grep.inc ok");
  }
  close OUT or die("Error $? closing $out: $!");
EOF


--let $_ag_outcome= `SELECT LOAD_FILE('$_AG_OUT')`
if ($_ag_outcome != 'assert_grep.inc ok')
{
  --source include/show_rpl_debug_info.inc
  --echo include/assert_grep.inc failed!
  --echo assert_text: '$assert_text'
  --echo assert_file: '$assert_file'
  --echo assert_select: '$assert_select'
  --echo assert_match: '$assert_match'
  --echo assert_count: '$assert_count'
  --echo assert_only_after: '$assert_only_after'
  if ($assert_match != '')
  {
    --echo matching lines: '$_ag_outcome'
  }
  if ($assert_count != '')
  {
    --echo number of matching lines: $_ag_outcome
  }
  --die assert_grep.inc failed.
}

--remove_file $_AG_OUT
--let $include_filename= include/assert_grep.inc [$assert_text]
--source include/end_include_file.inc
