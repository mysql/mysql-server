#!/usr/bin/perl
############################################################################
#     Stress test for MySQL/InnoDB combined database
#     (c) 2002 Innobase Oy & MySQL AB
#
############################################################################

use Cwd;
use DBI;
use Benchmark;

$opt_loop_count = 100000;

$pwd = cwd(); $pwd = "." if ($pwd eq '');
require "$pwd/bench-init.pl" || die "Can't read Configuration file: $!\n";

print "Innotest2: MySQL/InnoDB stress test in Perl for FOREIGN keys\n";
print "------------------------------------------------------------\n";
print "This is a randomized stress test for concurrent inserts,\n";
print "updates, deletes, commits and rollbacks with foreign keys with\n";
print "the ON DELETE ... clause. The test will generate\n";
print "also a lot of deadlocks, duplicate key errors, and other SQL errors.\n";
print "\n";
print "You should run innotest2, innotest2a, and innotest2b concurrently.\n";
print "The thing to watch is that the server does not crash or does not\n";
print "print to the .err log anything. Currently, due to a buglet in MySQL,\n";
print "warnings about MySQL lock reservations can appear in the .err log.\n";
print "The test will run very long, even several hours. You can kill\n";
print "the perl processes running this test at any time and do CHECK\n";
print "TABLE on tables innotest2a, b, c, d in the 'test' database.\n";
print "\n";
print "Some of these stress tests will print a lot of SQL errors\n";
print "to the standard output. That is not to be worried about.\n";
print "You can direct the output to a file like this:\n";
print "perl innotest2 > out2\n\n";

print "Generating random keys\n";
$random[$opt_loop_count] = 0;
$rnd_str[$opt_loop_count] = "a";

for ($i = 0; $i < $opt_loop_count; $i++) {

	$random[$i] = ($i * 63857) % $opt_loop_count;

  	if (0 == ($random[$i] % 3)) {
  		$rnd_str[$i] = "khD";
	} else { if (1 == ($random[$i] % 3)) {
		$rnd_str[$i] = "khd";
	} else { if (2 == ($random[$i] % 3)) {
		$rnd_str[$i] = "kHd";
	}}}

	for ($j = 0; $j < (($i * 764877) % 10); $j++) {
		$rnd_str[$i] = $rnd_str[$i]."k";
	}	
}

####
####  Connect
####

$dbh = $server->connect()
|| die $dbh->errstr;

$dbh->do("set autocommit = 0");

for ($i = 0; $i < 1; $i++) {
	print "loop $i\n";

	print "dropping table innotest2a\n";
	$dbh->do("drop table innotest2a");

	print "dropping table innotest2b\n";
	$dbh->do("drop table innotest2b");

	print "dropping table innotest2c\n";
	$dbh->do("drop table innotest2c");

	print "dropping table innotest2d\n";
	$dbh->do("drop table innotest2d");

	print "creating table innotest2b\n";
	$dbh->do(
	"create table innotest2b (A INT NOT NULL AUTO_INCREMENT, D INT NOT NULL, B VARCHAR(200) NOT NULL, C VARCHAR(175), PRIMARY KEY (A, D, B), INDEX (B, C), INDEX (C)) TYPE = INNODB")
	|| die $dbh->errstr;	

	print "creating table innotest2a\n";

	$dbh->do(
	"create table innotest2a (A INT NOT NULL AUTO_INCREMENT, D INT NOT NULL, B VARCHAR(200) NOT NULL, C VARCHAR(175), PRIMARY KEY (A, D, B), INDEX (B, C), INDEX (C), FOREIGN KEY (A, D) REFERENCES innotest2b (A, D) ON DELETE CASCADE) TYPE = INNODB")
	|| die $dbh->errstr;

	print "creating table innotest2c\n";

	$dbh->do(
	"create table innotest2c (A INT NOT NULL AUTO_INCREMENT, D INT NOT NULL, B VARCHAR(200) NOT NULL, C VARCHAR(175), PRIMARY KEY (A, D, B), INDEX (B, C), INDEX (C), FOREIGN KEY (A, D) REFERENCES innotest2a (A, D) ON DELETE CASCADE, FOREIGN KEY (B, C) REFERENCES innotest2a (B, C) ON DELETE CASCADE) TYPE = INNODB")
	|| die $dbh->errstr;	

	print "creating table innotest2d\n";

	$dbh->do(
	"create table innotest2d (A INT AUTO_INCREMENT, D INT, B VARCHAR(200), C VARCHAR(175), UNIQUE KEY (A, D, B), INDEX (B, C), INDEX (C), FOREIGN KEY (C) REFERENCES innotest2c (C) ON DELETE SET NULL, FOREIGN KEY (B, C) REFERENCES innotest2c (B, C) ON DELETE SET NULL) TYPE = INNODB")
	|| die $dbh->errstr;	
	print "created\n";

	for ($j = 0; $j < $opt_loop_count - 10; $j = $j + 2) {
		$dbh->do(
		"insert into innotest2b (D, B, C) values (5, '".$rnd_str[$j]."' ,'".$rnd_str[$j]."')")
		|| print $dbh->errstr;

		$dbh->do(
		"insert into innotest2a (D, B, C) values (5, '".$rnd_str[$j]."' ,'".$rnd_str[$j]."')")
		|| print $dbh->errstr;
		
		$dbh->do(
		"insert into innotest2c (D, B, C) values (5, '".$rnd_str[$j]."' ,'".$rnd_str[$j]."')")
		|| print $dbh->errstr;

		$dbh->do(
		"insert into innotest2d (D, B, C) values (5, '".$rnd_str[$j]."' ,'".$rnd_str[$j]."')")
		|| print $dbh->errstr;

		$dbh->do("delete from innotest2b where A = ".$random[$random[$j]])
		|| print $dbh->errstr;
		
		if (0 == ($j % 10)) {
			$dbh->do("commit");
		}

		if (0 == ($j % 39)) {
			$dbh->do("rollback");
		}		

		if (0 == ($j % 1000)) {
			print "round $j\n";
		}
		if (0 == ($j % 20000)) {
			print "Checking tables...\n";
			$dbh->do("check table innotest2a");
			$dbh->do("check table innotest2b");
			$dbh->do("check table innotest2c");
			$dbh->do("check table innotest2d");
			print "Tables checked.\n";
		}
	}	
	
	$dbh->do("commit");
}				

$dbh->disconnect;				# close connection
