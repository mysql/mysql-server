#
# test range scan bounds
# give option --all to test all cases
# set MYSQL_HOME to installation top
#

use strict;
use integer;
use Getopt::Long;
use DBI;

my $opt_all = 0;
my $opt_cnt = 5;
my $opt_verbose = 0;
GetOptions("all" => \$opt_all, "cnt=i" => \$opt_cnt, "verbose" => \$opt_verbose)
  or die "options are:  --all --cnt=N --verbose";

my $mysql_home = $ENV{MYSQL_HOME};
defined($mysql_home) or die "no MYSQL_HOME";
my $dsn = "dbi:mysql:database=test;host=localhost;mysql_read_default_file=$mysql_home/var/my.cnf";
my $opts = { RaiseError => 0, PrintError => 0, AutoCommit => 1, };

my $dbh;
my $sth;
my $sql;

$dbh = DBI->connect($dsn, "root", undef, $opts) or die $DBI::errstr;

my $table = 't';

$sql = "drop table if exists $table";
$dbh->do($sql) or die $DBI::errstr;

sub cut ($$$) {
  my($op, $key, $val) = @_;
  $op = '==' if $op eq '=';
  my(@w) = @$val;
  eval "\@w = grep(\$_ $op $key, \@w)";
  $@ and die $@;
  return [ @w ];
}

sub mkdummy ($) {
  my ($val) = @_;
  return {
    'dummy' => 1,
    'exp' => '9 = 9',
    'res' => $val,
  };
}

sub mkone ($$$$) {
  my($col, $op, $key, $val) = @_;
  my $res = cut($op, $key, $val);
  return {
    'exp' => "$col $op $key",
    'res' => $res,
  };
}

sub mktwo ($$$$$$) {
  my($col, $op1, $key1, $op2, $key2, $val) = @_;
  my $res = cut($op2, $key2, cut($op1, $key1, $val));
  return {
    'exp' => "$col $op1 $key1 and $col $op2 $key2",
    'res' => $res,
  };
}

sub mkall ($$$$) {
  my($col, $key1, $key2, $val) = @_;
  my @a = ();
  my $p = mkdummy($val);
  push(@a, $p) if $opt_all;
  my @ops = qw(< <= = >= >);
  for my $op (@ops) {
    my $p = mkone($col, $op, $key1, $val);
    push(@a, $p) if $opt_all || @{$p->{res}} != 0;
  }
  my @ops1 = $opt_all ? @ops : qw(= >= >);
  my @ops2 = $opt_all ? @ops : qw(<= <);
  for my $op1 (@ops1) {
    for my $op2 (@ops2) {
      my $p = mktwo($col, $op1, $key1, $op2, $key2, $val);
      push(@a, $p) if $opt_all || @{$p->{res}} != 0;
    }
  }
  warn scalar(@a)." cases\n" if $opt_verbose;
  return \@a;
}

my $casecnt = 0;

sub verify ($$$) {
  my($sql, $ord, $res) = @_;
  warn "$sql\n" if $opt_verbose;
  $sth = $dbh->prepare($sql) or die "prepare: $sql: $DBI::errstr";
  $sth->execute() or die "execute: $sql: $DBI::errstr";
  #
  # BUG: execute can return success on error so check again
  #
  $sth->err and die "execute: $sql: $DBI::errstr";
  my @out = ();
  for my $b (@{$res->[0]}) {
    for my $c (@{$res->[1]}) {
      for my $d (@{$res->[2]}) {
	push(@out, [$b, $c, $d]);
      }
    }
  }
  if ($ord) {
    @out = sort {
      $ord * ($a->[0] - $b->[0]) ||
      $ord * ($a->[1] - $b->[1]) ||
      $ord * ($a->[2] - $b->[2]) ||
      0
    } @out;
  }
  my $cnt = scalar @out;
  my $n = 0;
  while (1) {
    my $row = $sth->fetchrow_arrayref;
    $row || last;
    @$row == 3 or die "bad row: $sql:  @$row";
    for my $v (@$row) {
      $v =~ s/^\s+|\s+$//g;
      $v =~ /^\d+$/ or die "bad value: $sql:  $v";
    }
    if ($ord) {
      my $out = $out[$n];
      $row->[0] == $out->[0] &&
      $row->[1] == $out->[1] &&
      $row->[2] == $out->[2] or
        die "$sql: row $n: got row @$row != @$out";
    }
    $n++;
  }
  $sth->err and die "fetch: $sql: $DBI::errstr";
  $n == $cnt or die "verify: $sql: got row count $n != $cnt";
  $casecnt++;
}

for my $nn ("bcd", "") {
  my %nn;
  for my $x (qw(b c d)) {
    $nn{$x} = $nn =~ /$x/ ? "not null" : "null";
  }
  warn "create table\n";
  $sql = <<EOF;
create table $table (
  a int primary key,
  b int $nn{b},
  c int $nn{c},
  d int $nn{d},
  index (b, c, d)
) engine=ndb
EOF
  $dbh->do($sql) or die $DBI::errstr;
  warn "insert\n";
  $sql = "insert into $table values(?, ?, ?, ?)";
  $sth = $dbh->prepare($sql) or die $DBI::errstr;
  my @val = (0..($opt_cnt-1));
  my $v0 = 0;
  for my $v1 (@val) {
    for my $v2 (@val) {
      for my $v3 (@val) {
	$sth->bind_param(1, $v0) or die $DBI::errstr;
	$sth->bind_param(2, $v1) or die $DBI::errstr;
	$sth->bind_param(3, $v2) or die $DBI::errstr;
	$sth->bind_param(4, $v3) or die $DBI::errstr;
	$sth->execute or die $DBI::errstr;
	$v0++;
      }
    }
  }
  warn "generate cases\n";
  my $key1 = 1;
  my $key2 = 3;
  my $a1 = mkall('b', $key1, $key2, \@val);
  my $a2 = mkall('c', $key1, $key2, \@val);
  my $a3 = mkall('d', $key1, $key2, \@val);
  warn "select\n";
  for my $ord (0, +1, -1) {
    my $orderby =
      $ord == 0 ? "" :
      $ord == +1 ? " order by b, c, d" :
      $ord == -1 ? " order by b desc, c desc, d desc" : die "not here";
    for my $p1 (@$a1) {
      my $res = [ $p1->{res}, \@val, \@val ];
      $sql = "select b, c, d from $table" .
	     " where $p1->{exp}" .
	     $orderby;
      verify($sql, $ord, $res);
      for my $p2 (@$a2) {
	my $res = [ $p1->{res}, $p2->{res}, \@val ];
	$sql = "select b, c, d from $table" .
	       " where $p1->{exp} and $p2->{exp}" .
	       $orderby;
	verify($sql, $ord, $res);
	for my $p3 (@$a3) {
	  my $res = [ $p1->{res}, $p2->{res}, $p3->{res} ];
	  $sql = "select b, c, d from $table" .
		 " where $p1->{exp} and $p2->{exp} and $p3->{exp}" .
		 $orderby;
	  verify($sql, $ord, $res);
	}
      }
    }
  }
  warn "drop table\n";
  $sql = "drop table $table";
  $dbh->do($sql) or die $DBI::errstr;
}

warn "verified $casecnt cases\n";
warn "done\n";

# vim: set sw=2:
