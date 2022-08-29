# "subroutine" in Perl.
# Function: extract the timing from an EXPLAIN ANALYZE and record the result
#           for further use by the calling script in an SQL table.
#
# Input: SQL command(s) in the file named by the environment variable COMMANDS
#        The file should contain one EXPLAIN ANALYZE statement, possibly
#        preceded by other commands (that shoudl not generate output), notably
#        (for our present use case), SET variable commands.
# Output: Actual time as found in EXPLAIN ANALYZE inserted into the table
#        timings in column t. The table is presumed created by caller prior to
#        invoking the subroutine. It also copies the input SQL commands to
#        stdout.
#
--perl
use strict;
my $commands = $ENV{'COMMANDS'};
my $mysql = $ENV{'MYSQL'};
my $master_myport = $ENV{'MASTER_MYPORT'};
my $sqlresult =
  ` $mysql \\
   --host=127.0.0.1 \\
   --port=$master_myport \\
   --user=root test \\
   < $commands`;
system("cat $commands");
# The output looks like this:
#
# EXPLAIN ANALYZE
# line 1\n line 2\n ... \n line N
#
# so first split on newline to get at the output lines
my @explain = split (/\n/, $sqlresult);
# next, split on backslash n to get the individual lines in an array
my @explain = split (/\\n/, $explain[1]);
# The first line is now in $explain[0] and should look like this:
#
# -> Table scan on <intersect temporary>  (cost=0.01..1599.17 rows=127734) (actual time=0.003..17.067 rows=65536 loops=1)
#
# Extract the line:
my $explain = $explain[0];
# so now extract the cost from that line and insert into timings table.
$explain =~ /actual time=[0-9]+[.][0-9]+[.][.]([0-9]+[.][0-9]+)/;
my $query = "INSERT INTO timings(t) VALUES($1)";
my $rc =
  ` $mysql \\
   --host=127.0.0.1 \\
   --port=$master_myport \\
   --user=root test \\
   --execute "$query" `;
EOF
