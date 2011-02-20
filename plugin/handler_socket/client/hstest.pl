#!/usr/bin/perl

# vim:sw=8:ai:ts=8

use strict;
use warnings;

use DBI;
use Net::HandlerSocket;

my %conf = ();
for my $i (@ARGV) {
	my ($k, $v) = split(/=/, $i);
	$conf{$k} = $v;
}

my $verbose = get_conf("verbose", 0);
my $actions_str = get_conf("actions", "hsread");
my $tablesize = get_conf("tablesize", 10000);
my $db = get_conf("db", "hstest");
my $table = get_conf("table", "hstest_table1");
my $engine = get_conf("engine", "innodb");
my $host = get_conf("host", "localhost");
my $mysqlport = get_conf("mysqlport", 3306);
my $mysqluser = get_conf("mysqluser", "root");
my $mysqlpass = get_conf("mysqlpass", "");
my $hsport = get_conf("hsport", 9999);
my $loop = get_conf("loop", 10000);
my $op = get_conf("op", "=");
my $ssps = get_conf("ssps", 0);
my $num_moreflds = get_conf("moreflds", 0);
my $moreflds_prefix = get_conf("moreflds_prefix", "column0123456789_");
my $keytype = get_conf("keytype", "varchar(32)");
my $file = get_conf("file", undef);

my $dsn = "DBI:mysql:database=;host=$host;port=$mysqlport"
	. ";mysql_server_prepare=$ssps";
my $dbh = DBI->connect($dsn, $mysqluser, $mysqlpass, { RaiseError => 1 });
my $hsargs = { 'host' => $host, 'port' => $hsport };
my $cli = new Net::HandlerSocket($hsargs);

my @actions = split(/,/, $actions_str);
for my $action (@actions) {
	if ($action eq "table") {
		print("TABLE $db.$table\n");
		$dbh->do("drop database if exists $db");
		$dbh->do("create database $db");
		$dbh->do("use $db");
		my $moreflds = get_createtbl_moreflds_str();
		$dbh->do(
			"create table $table (" .
			"k $keytype primary key" .
			",v varchar(32) not null" .
			$moreflds .
			") character set utf8 collate utf8_bin " .
			"engine = $engine");
	} elsif ($action eq "insert") {
		print("INSERT $db.$table tablesize=$tablesize\n");
		$dbh->do("use $db");
		my $moreflds = get_insert_moreflds_str();
		for (my $i = 0; $i < $tablesize; $i += 100) {
			my $qstr = "insert into $db.$table values";
			for (my $j = 0; $j < 100; ++$j) {
				if ($j != 0) {
					$qstr .= ",";
				}
				my $k = "" . ($i + $j);
				my $v = "v" . int(rand(1000)) . ($i + $j);
				$qstr .= "('$k', '$v'";
				for (my $j = 0; $j < $num_moreflds; ++$j) {
					$qstr .= ",'$j'";
				}
				$qstr .= ")";
			}
			$dbh->do($qstr);
			print "$i/$tablesize\n" if $i % 1000 == 0;
		}
	} elsif ($action eq "read") {
		print("READ $db.$table op=$op loop=$loop\n");
		$dbh->do("use $db");
		my $moreflds = get_select_moreflds_str();
		my $sth = $dbh->prepare(
			"select k,v$moreflds from $db.$table where k = ?");
		for (my $i = 0; $i < $loop; ++$i) {
			my $k = "" . int(rand($tablesize));
			# print "k=$k\n";
			$sth->execute($k);
			if ($verbose >= 10) {
				print "RET:";
				while (my $ref = $sth->fetchrow_arrayref()) {
					my $rk = $ref->[0];
					my $rv = $ref->[1];
					print " $rk $rv";
				}
				print "\n";
			}
			print "$i/$loop\n" if $i % 1000 == 0;
		}
	} elsif ($action eq "hsinsert") {
		print("HSINSERT $db.$table tablesize=$tablesize\n");
		$cli->open_index(1, $db, $table, '', 'k,v');
		for (my $i = 0; $i < $tablesize; ++$i) {
			my $k = "" . $i;
			my $v = "v" . int(rand(1000)) . $i;
			my $r = $cli->execute_insert(1, [ $k, $v ]);
			if ($r->[0] != 0) {
				die;
			}
			print "$i/$tablesize\n" if $i % 1000 == 0;
		}
	} elsif ($action eq "hsread") {
		print("HSREAD $db.$table op=$op loop=$loop\n");
		my $moreflds = get_select_moreflds_str();
		$cli->open_index(1, $db, $table, '', "k,v$moreflds");
		for (my $i = 0; $i < $loop; ++$i) {
			my $k = "" . int(rand($tablesize));
			# print "k=$k\n";
			my $r = $cli->execute_find(1, $op, [ $k ], 1, 0);
			if ($verbose >= 10) {
				my $len = scalar(@{$r});
				print "LEN=$len";
				for my $e (@{$r}) {
					print " [$e]";
				}
				print "\n";
			}
			print "$i/$loop\n" if $i % 1000 == 0;
		}
	} elsif ($action eq "hsupdate") {
		my $vbase = "v" . int(rand(1000));
		print("HSUPDATE $db.$table op=$op loop=$loop vbase=$vbase\n");
		$cli->open_index(1, $db, $table, '', 'v');
		for (my $i = 0; $i < $loop; ++$i) {
			my $k = "" . int(rand($tablesize));
			my $v = $vbase . $i;
			print "k=$k v=$v\n";
			my $r = $cli->execute_update(1, $op, [ $k ], 1, 0,
				[ $v ]);
			if ($verbose >= 10) {
				print "UP k=$k v=$v\n";
			}
			print "$i/$loop\n" if $i % 1000 == 0;
		}
	} elsif ($action eq "hsdelete") {
		print("HSDELETE $db.$table op=$op loop=$loop\n");
		$cli->open_index(1, $db, $table, '', '');
		for (my $i = 0; $i < $loop; ++$i) {
			my $k = "" . int(rand($tablesize));
			print "k=$k\n";
			my $r = $cli->execute_delete(1, $op, [ $k ], 1, 0);
			if ($verbose >= 10) {
				print "DEL k=$k\n";
			}
			print "$i/$loop\n" if $i % 1000 == 0;
		}
	} elsif ($action eq "verify") {
		verify_do();
	}
}

sub verify_do {
	my ($fail_cnt, $ok_cnt) = (0, 0);
	my $sth = $dbh->prepare("select v from $db.$table where k = ?");
	use FileHandle;
	my $fh = new FileHandle($file, "r");
	while (my $line = <$fh>) {
		chomp($line);
		my @vec = split(/\t/, $line);
		my $k = $vec[3];
		my $v = $vec[7];
		next if (!defined($k) || !defined($v));
		# print "$k $v\n";
		$sth->execute($k);
		my $aref = $sth->fetchrow_arrayref();
		if (!defined($aref)) {
			print "FAILED: $k notfound\n";
			++$fail_cnt;
		} else {
			my $gv = $aref->[0];
			if ($gv ne $v) {
				print "FAILED: $k got=$gv expected=$v\n";
				++$fail_cnt;
			} else {
				print "OK: $k $v $gv\n" if $verbose >= 10;
				++$ok_cnt;
			}
		}
	}
	print "OK=$ok_cnt FAIL=$fail_cnt\n";
}

sub get_conf {
	my ($key, $def) = @_;
	my $val = $conf{$key};
	if ($val) {
		print "$key=$val\n";
	} else {
		$val = $def;
		$def ||= '';
		print "$key=$def(default)\n";
	}
	return $val;
}

sub get_createtbl_moreflds_str {
	my $s = "";
	for (my $j = 0; $j < $num_moreflds; ++$j) {
		$s .= ",$moreflds_prefix$j varchar(30)";
	}
	return $s;
}

sub get_select_moreflds_str {
	my $s = "";
	for (my $i = 0; $i < $num_moreflds; ++$i) {
		$s .= ",$moreflds_prefix$i";
	}
	return $s;
}

sub get_insert_moreflds_str {
	my $s = "";
	for (my $i = 0; $i < $num_moreflds; ++$i) {
		$s .= ",?";
	}
	return $s;
}

