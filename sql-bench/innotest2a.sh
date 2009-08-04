#!/usr/bin/perl
############################################################################
#     Stress test for MySQL/Innobase combined database
#     (c) 2000 Innobase Oy & MySQL AB
#
############################################################################

use Cwd;
use DBI;
use Benchmark;

$opt_loop_count = 100000;

$pwd = cwd(); $pwd = "." if ($pwd eq '');
require "$pwd/bench-init.pl" || die "Can't read Configuration file: $!\n";

print "Innotest2a: MySQL/InnoDB stress test in Perl for FOREIGN keys\n";
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
		$rnd_str[$i] = "kHd";
	} else { if (2 == ($random[$i] % 3)) {
		$rnd_str[$i] = "khd";
	}}}

	for ($j = 0; $j < (($i * 764877) % 20); $j++) {
		$rnd_str[$i] = $rnd_str[$i]."k";
	}	
}

####
####  Connect
####

$dbh = $server->connect()
|| die $dbh->errstr;

$dbh->do("set autocommit = 0");

for ($i = 0; $i < 5; $i++) {
	print "loop $i\n";

	for ($j = 0; $j < $opt_loop_count - 10; $j = $j + 1) {
		
		$dbh->do("update innotest2a set B = '".$rnd_str[$j + 1]."' where A = ".$random[$j + 5])
		|| print $dbh->errstr;

		$dbh->do("delete from innotest2a where A = ".$random[$random[$j]])
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
	}	
	
	$dbh->do("commit");
}				

$dbh->disconnect;				# close connection
