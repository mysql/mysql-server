#! /usr/bin/perl

# Read the output of async_queries.c. Run the queries again serially, using
# the normal (not asynchronous) API. Compare the two results for correctness.

use strict;
use warnings;

use DBI;

my $D= [];

die "Usage: $0 <host> <user> <password> <database>\n"
    unless @ARGV == 4;

my $dbh= DBI->connect("DBI:mysql:database=$ARGV[3];host=$ARGV[0]",
                      $ARGV[1], $ARGV[2],
                      { RaiseError => 1, PrintError => 0 });

while (<STDIN>) {
  chomp;
  if (/^([0-9]+) ! (.*);$/) {
    my ($index, $query)= ($1, $2);
    $D->[$index]= { QUERY => $query, OUTPUT => [] };
  } elsif (/^([0-9]+) - (.*)$/) {
    my ($index, $data)= ($1, $2);
    push @{$D->[$index]{OUTPUT}}, $data;
  } elsif (/^([0-9]+) \| Error: (.*)$/) {
    my ($index, $errmsg)= ($1, $2);
    my $rows;
    my $res= eval {
      my $stm= $dbh->prepare($D->[$index]{QUERY});
      $stm->execute();
      $rows= $stm->fetchall_arrayref();
      1;
    };
    if ($res) {
      die "Query $index succeeded, but should have failed with error.\nquery=$D->[$index]{QUERY}\nerror=$errmsg\n";
    }
    my $errmsg2= $@;
    if ($errmsg2 =~ /^DBD::.*failed: (.*) at .*$/s) {
      $errmsg2= $1;
    } else {
      die "Unexpected DBD error message format: '$errmsg2'\n";
    }
    if ($errmsg2 ne $errmsg) {
      die "Query $index failed with different error message\nquery=$D->[$index]{QUERY}\nerror1=$errmsg\nerror2=$errmsg2\n";
    }
    print "OK $index\n";
    delete $D->[$index];
  } elsif (/^([0-9]+) \| EOF$/) {
    my $index= $1;
    my $rows;
    my $res= eval {
      my $stm= $dbh->prepare($D->[$index]{QUERY});
      $stm->execute();
      $rows= $stm->fetchall_arrayref();
      1;
    };
    if (!$res) {
      die "Query $index failed, but should have succeeded.\nquery=$D->[$index]{QUERY}\nerror=$@\n";
    }
    my $result_string= join("\n", sort @{$D->[$index]{OUTPUT}});
    my $result_string2= join("\n", sort(map(join("\t", map((defined($_) ? $_ : "(null)"), @$_)), @$rows)));
    if ($result_string ne $result_string2) {
      die "Query $index result difference.\nquery=$D->[$index]{QUERY}\noutput1=\n$$result_string\noutput2=\n$result_string2\n";
    }
    delete $D->[$index];
  } else {
    die "Unexpected line: '$_'\n";
  }
}
$dbh->disconnect();
