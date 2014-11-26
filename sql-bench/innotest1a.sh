#!/usr/bin/perl
############################################################################
#     Stress test for MySQL/InnoDB combined database
#     (c) 2002 Innobase Oy & MySQL AB
#
############################################################################

use Cwd;
use DBI;
use Benchmark;

$opt_loop_count = 200000;

$pwd = cwd(); $pwd = "." if ($pwd eq '');
require "$pwd/bench-init.pl" || die "Can't read Configuration file: $!\n";

print "Innotest1a: MySQL/InnoDB stress test in Perl\n";
print "-------------------------------------------\n";
print "This is a randomized stress test for concurrent inserts,\n";
print "updates, deletes, commits and rollbacks. The test will generate\n";
print "also a lot of deadlocks, duplicate key errors, and other SQL errors.\n";
print "\n";
print "You should run innotest1, innotest1a, and innotest1b concurrently.\n";
print "The thing to watch is that the server does not crash or does not\n";
print "print to the .err log anything. Currently, due to a buglet in MySQL,\n";
print "warnings about MySQL lock reservations can appear in the .err log.\n";
print "The test will run very long, even several hours. You can kill\n";
print "the perl processes running this test at any time and do CHECK\n";
print "TABLE on table innotest1 in the 'test' database.\n";
print "\n";
print "Some of these stress tests will print a lot of SQL errors\n";
print "to the standard output. That is not to be worried about.\n";
print "You can direct the output to a file like this:\n";
print "perl innotest1 > out1\n\n";

print "Generating random keys\n";
$random[$opt_loop_count] = 0;
$rnd_str[$opt_loop_count] = "a";

for ($i = 0; $i < $opt_loop_count; $i++) {

	$random[$i] = ($i * 63857) % $opt_loop_count;

  	if (0 == ($random[$i] % 3)) {
  		$rnd_str[$i] = "kjgclgrtfuylfluyfyufyulfulfyyulofuyolfyufyufuyfyufyufyufyufyyufujhfghd";
	} else { if (1 == ($random[$i] % 3)) {
		$rnd_str[$i] = "khd";
	} else { if (2 == ($random[$i] % 3)) {
		$rnd_str[$i] = "kh";
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

for ($i = 0; $i < 1; $i++) {
	print "loop $i\n";

	for ($j = 1; $j < $opt_loop_count - 10; $j = $j + 2) {
		$dbh->do(
		"insert into innotest1 (D, B, C) values (5, '".$rnd_str[$j]."' ,'".$rnd_str[$j]."')");
		$dbh->do("update innotest1 set B = '".$rnd_str[$j + 2]."' where A = ".$random[$j + 5]);
		$dbh->do("update innotest1 SET D = D + 1 where A =".(($j - 1) / 2 - 777));
		$dbh->do("update innotest1 set B = '".$rnd_str[$j + 8]."' where A =".(($j - 1) / 2 - 770));
		$dbh->do("delete from innotest1 where A = ".$random[$random[$j]]);

		fetch_all_rows($dbh, "select b, c from innotest1 where a > ".$random[$j]." and a < ".($random[$j] + 7));
		
		if (0 == ($j % 37)) {
			$dbh->do("commit");
		}

		if (0 == ($j % 533)) {
			$dbh->do("rollback");
		}		

		if (0 == ($j % 537)) {
		print fetch_all_rows($dbh, "select c from innotest1 where c = '".$rnd_str[$j - 67]."'");
		print fetch_all_rows($dbh, "select b from innotest1 where b = '".$rnd_str[$j - 688]."'");
		print fetch_all_rows($dbh, "select b, c from innotest1 where c = '".$rnd_str[$j - 67]."'");
		print fetch_all_rows($dbh, "select b, c from innotest1 where b = '".$rnd_str[$j - 622]."'");
		print fetch_all_rows($dbh, "select a, b, c from innotest1 where c = '".$rnd_str[$j - 68]."'");
		print fetch_all_rows($dbh, "select a, b, c from innotest1 where b = '".$rnd_str[$j - 644]."'");
		print fetch_all_rows($dbh, "select d, b, c from innotest1 where c = '".$rnd_str[$j - 68]."'");
		print fetch_all_rows($dbh, "select d, b, c from innotest1 where b = '".$rnd_str[$j - 677]."'");
		print "\n";
		}

		if (0 == (($j - 1) % 1000)) {
			print "round $j\n";
		}
	}	
	
	$dbh->do("commit");
}				

$dbh->disconnect;				# close connection
