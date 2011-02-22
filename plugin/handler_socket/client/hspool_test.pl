#!/usr/bin/perl

use strict;
use warnings;
use DB::HandlerSocket::Pool;
use DBI;

my %conf = ();
for my $i (@ARGV) {
	my ($k, $v) = split(/=/, $i);
	$conf{$k} = $v;
}

my $verbose = get_conf("verbose", 0);
my $actions_str = get_conf("actions",
	"create,insert,verify,verify2,verify3,verify4,clean");
my $tablesize = get_conf("tablesize", 1000);
my $db = get_conf("db", "hstestdb");
my $table = get_conf("table", "testtbl");
my $table_schema = get_conf("table_schema", undef);
my $engine = get_conf("engine", "innodb");
my $host = get_conf("host", "localhost");
my $mysqlport = get_conf("mysqlport", 3306);
my $hsport_rd = get_conf("hsport_rd", 9998);
my $hsport_wr = get_conf("hsport_wr", 9999);
my $loop = get_conf("loop", 10000);
my $op = get_conf("op", "=");
my $ssps = get_conf("ssps", 0);
my $num_moreflds = get_conf("moreflds", 0);
my $moreflds_prefix = get_conf("moreflds_prefix", "f");
my $mysql_user = 'root';
my $mysql_password = '';

my $dsn = "DBI:mysql:database=;host=$host;port=$mysqlport"
        . ";mysql_server_prepare=$ssps";
my $dbh = DBI->connect($dsn, $mysql_user, $mysql_password,
	{ RaiseError => 1 });
my $hsargs = { 'host' => $host, 'port' => $hsport_rd };
my $hspool = new DB::HandlerSocket::Pool({
	hostmap => {
		"$db.$table" => {
			host => $host,
			port => $hsport_rd,
		},
	},
	resolve => undef,
	error => undef,
});
$table_schema = "(k int primary key, fc30 varchar(30), ft text)"
	if (!defined($table_schema));

my @actions = split(/,/, $actions_str);
for my $action (@actions) {
	print "ACTION: $action\n";
	eval "hstest_$action()";
	if ($@) {
		die $@;
	}
	print "ACTION: $action DONE\n";
}

sub get_conf {
	my ($key, $def) = @_;
	my $val = $conf{$key};
	if ($val) {
		print "$key=$val\n";
	} else {
		$val = $def;
		my $defstr = $def || "(undef)";
		print "$key=$defstr(default)\n";
	}
	return $val;
}

sub hstest_create {
	$dbh->do("drop database if exists $db");
	$dbh->do("create database $db");
	$dbh->do("use $db");
	$dbh->do("create table $table $table_schema engine=$engine");
}

sub hstest_dump {
	$dbh->do("use $db");
	my $sth = $dbh->prepare("select * from $table");
	$sth->execute();
	my $arr = $sth->fetchall_arrayref();
	for my $rec (@$arr) {
		print "REC:";
		for my $row (@$rec) {
			print " $row";
		}
		print "\n";
	}
}

sub hstest_insert {
	$dbh->do("use $db");
	my $sth = $dbh->prepare("insert into $table values (?, ?, ?)");
	for (my $k = 0; $k < $tablesize; ++$k) {
		my $fc30 = "fc30_$k";
		my $ft = "ft_$k";
		$sth->execute($k, $fc30, $ft);
	}
}

sub hstest_verify {
	$dbh->do("use $db");
	my $sth = $dbh->prepare("select * from $table order by k");
	$sth->execute();
	my $arr = $sth->fetchall_arrayref();
	my $hsres = $hspool->index_find($db, $table, "PRIMARY", "k,fc30,ft",
		">=", [ 0 ], $tablesize, 0);
	for (my $i = 0; $i < $tablesize; ++$i) {
		my $rec = $arr->[$i];
		my $differ = 0;
		print "REC:" if $verbose;
		for (my $j = 0; $j < 3; ++$j) {
			my $fld = $rec->[$j];
			my $hsidx = $i * 3 + $j;
			my $hsfld = $hsres->[$hsidx];
			if ($hsfld ne $fld) {
				$differ = 1;
			}
			if ($differ) {
				print " $fld:$hsfld" if $verbose;
			} else {
				print " $hsfld" if $verbose;
			}
		}
		print "\n" if $verbose;
		if ($differ) {
			die "verification failed";
		}
	}
}

sub hstest_verify2 {
	$dbh->do("use $db");
	my $sth = $dbh->prepare("select * from $table order by k");
	$sth->execute();
	my $arr = $sth->fetchall_arrayref();
	my $hsresa = $hspool->index_find_multi($db, $table, "PRIMARY",
		"k,fc30,ft", [ [ -1, ">=", [ 0 ], $tablesize, 0 ] ]);
	my $hsres = $hsresa->[0];
	for (my $i = 0; $i < $tablesize; ++$i) {
		my $rec = $arr->[$i];
		my $differ = 0;
		print "REC:" if $verbose;
		for (my $j = 0; $j < 3; ++$j) {
			my $fld = $rec->[$j];
			my $hsidx = $i * 3 + $j;
			my $hsfld = $hsres->[$hsidx];
			if ($hsfld ne $fld) {
				$differ = 1;
			}
			if ($differ) {
				print " $fld:$hsfld" if $verbose;
			} else {
				print " $hsfld" if $verbose;
			}
		}
		print "\n" if $verbose;
		if ($differ) {
			die "verification failed";
		}
	}
}

sub hashref_to_str {
	my $href = $_[0];
	my $r = '';
	for my $k (sort keys %$href) {
		my $v = $href->{$k};
		$r .= "($k=>$v)";
	}
	return $r;
}

sub hstest_verify3 {
	$dbh->do("use $db");
	my $sth = $dbh->prepare("select * from $table order by k");
	$sth->execute();
	my $hsres_t = $hspool->index_find($db, $table, "PRIMARY", "k,fc30,ft",
			">=", [ 0 ], $tablesize, 0);
	my $hsres = DB::HandlerSocket::Pool::result_single_to_hasharr(
		[ 'k', 'fc30', 'ft' ], $hsres_t);
	for (my $i = 0; $i < $tablesize; ++$i) {
		my $mystr = hashref_to_str($sth->fetchrow_hashref());
		my $hsstr = hashref_to_str($hsres->[$i]);
		if ($mystr ne $hsstr) {
			print "DIFF my=[$mystr] hs=[$hsstr]\n" if $verbose;
			die "verification failed";
		} else {
			print "OK $hsstr\n" if $verbose;
		}
	}
}

sub hstest_verify4 {
	$dbh->do("use $db");
	my $sth = $dbh->prepare("select * from $table order by k");
	$sth->execute();
	my $hsres_t = $hspool->index_find($db, $table, "PRIMARY", "k,fc30,ft",
			">=", [ 0 ], $tablesize, 0);
	my $hsres = DB::HandlerSocket::Pool::result_single_to_hashhash(
		[ 'k', 'fc30', 'ft' ], 'k', $hsres_t);
	my $rechash = $sth->fetchall_hashref('k');
	while (my ($k, $href) = each (%$rechash)) {
		my $mystr = hashref_to_str($href);
		my $hsstr = hashref_to_str($hsres->{$k});
		if ($mystr ne $hsstr) {
			print "DIFF my=[$mystr] hs=[$hsstr]\n" if $verbose;
			die "verification failed";
		} else {
			print "OK $hsstr\n" if $verbose;
		}
	}
}

sub hstest_clean {
	$hspool->clear_pool();
	$dbh->do("drop database if exists $db");
}

