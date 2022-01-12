#!/usr/bin/perl -w

use strict;
use Getopt::Long;
use FileHandle;

my $infile;

GetOptions(
	"if=s" => \$infile
);

my $raw_data;
my @data;
my $dirname = `dirname $infile`;
chomp $dirname;
my $avg;

sub average()
{
	my $count = scalar( @data );
	my $total = 0;

	for ( my $i = 0; $i < $count; $i++ ) {
		$total += $data[ $i ];
	}

	return $total / $count;
}

# Generate data file for power query times, for the first iteration only.

system "grep 'PERF1.POWER.Q1 ' $infile | awk '{print \"1\", \$11}' > $dirname/q_time_p.data";
system "grep 'PERF1.POWER.Q2 ' $infile | awk '{print \"2\", \$11}' >> $dirname/q_time_p.data";
system "grep 'PERF1.POWER.Q3 ' $infile | awk '{print \"3\", \$11}' >> $dirname/q_time_p.data";
system "grep 'PERF1.POWER.Q4 ' $infile | awk '{print \"4\", \$11}' >> $dirname/q_time_p.data";
system "grep 'PERF1.POWER.Q5 ' $infile | awk '{print \"5\", \$11}' >> $dirname/q_time_p.data";
system "grep 'PERF1.POWER.Q6 ' $infile | awk '{print \"6\", \$11}' >> $dirname/q_time_p.data";
system "grep 'PERF1.POWER.Q7 ' $infile | awk '{print \"7\", \$11}' >> $dirname/q_time_p.data";
system "grep 'PERF1.POWER.Q8 ' $infile | awk '{print \"8\", \$11}' >> $dirname/q_time_p.data";
system "grep 'PERF1.POWER.Q9 ' $infile | awk '{print \"9\", \$11}' >> $dirname/q_time_p.data";
system "grep 'PERF1.POWER.Q10 ' $infile | awk '{print \"10\", \$11}' >> $dirname/q_time_p.data";
system "grep 'PERF1.POWER.Q11 ' $infile | awk '{print \"11\", \$11}' >> $dirname/q_time_p.data";
system "grep 'PERF1.POWER.Q12 ' $infile | awk '{print \"12\", \$11}' >> $dirname/q_time_p.data";
system "grep 'PERF1.POWER.Q13 ' $infile | awk '{print \"13\", \$11}' >> $dirname/q_time_p.data";
system "grep 'PERF1.POWER.Q14 ' $infile | awk '{print \"14\", \$11}' >> $dirname/q_time_p.data";
system "grep 'PERF1.POWER.Q15 ' $infile | awk '{print \"15\", \$11}' >> $dirname/q_time_p.data";
system "grep 'PERF1.POWER.Q16 ' $infile | awk '{print \"16\", \$11}' >> $dirname/q_time_p.data";
system "grep 'PERF1.POWER.Q17 ' $infile | awk '{print \"17\", \$11}' >> $dirname/q_time_p.data";
system "grep 'PERF1.POWER.Q18 ' $infile | awk '{print \"18\", \$11}' >> $dirname/q_time_p.data";
system "grep 'PERF1.POWER.Q19 ' $infile | awk '{print \"19\", \$11}' >> $dirname/q_time_p.data";
system "grep 'PERF1.POWER.Q20 ' $infile | awk '{print \"20\", \$11}' >> $dirname/q_time_p.data";
system "grep 'PERF1.POWER.Q21 ' $infile | awk '{print \"21\", \$11}' >> $dirname/q_time_p.data";
system "grep 'PERF1.POWER.Q22 ' $infile | awk '{print \"22\", \$11}' >> $dirname/q_time_p.data";
system "grep 'PERF1.POWER.RF1 ' $infile | awk '{print \"23\", \$11}' >> $dirname/q_time_p.data";
system "grep 'PERF1.POWER.RF2 ' $infile | awk '{print \"24\", \$11}' >> $dirname/q_time_p.data";

# Generate data file of average throughput query times.

$raw_data = `grep THRUPUT $infile | grep 'Q1 ' | awk '{print \$11}' | xargs`;
@data = split ' ', $raw_data;
$avg = average();
system "echo 1.1 $avg > $dirname/q_time_t.data";

$raw_data = `grep THRUPUT $infile | grep 'Q2 ' | awk '{print \$11}' | xargs`;
@data = split ' ', $raw_data;
$avg = average();
system "echo 2.1 $avg >> $dirname/q_time_t.data";

$raw_data = `grep THRUPUT $infile | grep 'Q3 ' | awk '{print \$11}' | xargs`;
@data = split ' ', $raw_data;
$avg = average();
system "echo 3.1 $avg >> $dirname/q_time_t.data";

$raw_data = `grep THRUPUT $infile | grep 'Q4 ' | awk '{print \$11}' | xargs`;
@data = split ' ', $raw_data;
$avg = average();
system "echo 4.1 $avg >> $dirname/q_time_t.data";

$raw_data = `grep THRUPUT $infile | grep 'Q5 ' | awk '{print \$11}' | xargs`;
@data = split ' ', $raw_data;
$avg = average();
system "echo 5.1 $avg >> $dirname/q_time_t.data";

$raw_data = `grep THRUPUT $infile | grep 'Q6 ' | awk '{print \$11}' | xargs`;
@data = split ' ', $raw_data;
$avg = average();
system "echo 6.1 $avg >> $dirname/q_time_t.data";

$raw_data = `grep THRUPUT $infile | grep 'Q7 ' | awk '{print \$11}' | xargs`;
@data = split ' ', $raw_data;
$avg = average();
system "echo 7.1 $avg >> $dirname/q_time_t.data";

$raw_data = `grep THRUPUT $infile | grep 'Q8 ' | awk '{print \$11}' | xargs`;
@data = split ' ', $raw_data;
$avg = average();
system "echo 8.1 $avg >> $dirname/q_time_t.data";

$raw_data = `grep THRUPUT $infile | grep 'Q9 ' | awk '{print \$11}' | xargs`;
@data = split ' ', $raw_data;
$avg = average();
system "echo 9.1 $avg >> $dirname/q_time_t.data";

$raw_data = `grep THRUPUT $infile | grep 'Q10 ' | awk '{print \$11}' | xargs`;
@data = split ' ', $raw_data;
$avg = average();
system "echo 10.1 $avg >> $dirname/q_time_t.data";

$raw_data = `grep THRUPUT $infile | grep 'Q11 ' | awk '{print \$11}' | xargs`;
@data = split ' ', $raw_data;
$avg = average();
system "echo 11.1 $avg >> $dirname/q_time_t.data";

$raw_data = `grep THRUPUT $infile | grep 'Q12 ' | awk '{print \$11}' | xargs`;
@data = split ' ', $raw_data;
$avg = average();
system "echo 12.1 $avg >> $dirname/q_time_t.data";

$raw_data = `grep THRUPUT $infile | grep 'Q13 ' | awk '{print \$11}' | xargs`;
@data = split ' ', $raw_data;
$avg = average();
system "echo 13.1 $avg >> $dirname/q_time_t.data";

$raw_data = `grep THRUPUT $infile | grep 'Q14 ' | awk '{print \$11}' | xargs`;
@data = split ' ', $raw_data;
$avg = average();
system "echo 14.1 $avg >> $dirname/q_time_t.data";

$raw_data = `grep THRUPUT $infile | grep 'Q15 ' | awk '{print \$11}' | xargs`;
@data = split ' ', $raw_data;
$avg = average();
system "echo 15.1 $avg >> $dirname/q_time_t.data";

$raw_data = `grep THRUPUT $infile | grep 'Q16 ' | awk '{print \$11}' | xargs`;
@data = split ' ', $raw_data;
$avg = average();
system "echo 16.1 $avg >> $dirname/q_time_t.data";

$raw_data = `grep THRUPUT $infile | grep 'Q17 ' | awk '{print \$11}' | xargs`;
@data = split ' ', $raw_data;
$avg = average();
system "echo 17.1 $avg >> $dirname/q_time_t.data";

$raw_data = `grep THRUPUT $infile | grep 'Q18 ' | awk '{print \$11}' | xargs`;
@data = split ' ', $raw_data;
$avg = average();
system "echo 18.1 $avg >> $dirname/q_time_t.data";

$raw_data = `grep THRUPUT $infile | grep 'Q19 ' | awk '{print \$11}' | xargs`;
@data = split ' ', $raw_data;
$avg = average();
system "echo 19.1 $avg >> $dirname/q_time_t.data";

$raw_data = `grep THRUPUT $infile | grep 'Q20 ' | awk '{print \$11}' | xargs`;
@data = split ' ', $raw_data;
$avg = average();
system "echo 20.1 $avg >> $dirname/q_time_t.data";

$raw_data = `grep THRUPUT $infile | grep 'Q21 ' | awk '{print \$11}' | xargs`;
@data = split ' ', $raw_data;
$avg = average();
system "echo 21.1 $avg >> $dirname/q_time_t.data";

$raw_data = `grep THRUPUT $infile | grep 'Q22 ' | awk '{print \$11}' | xargs`;
@data = split ' ', $raw_data;
$avg = average();
system "echo 22.1 $avg >> $dirname/q_time_t.data";

$raw_data = `grep THRUPUT $infile | grep 'RF1 ' | awk '{print \$11}' | xargs`;
@data = split ' ', $raw_data;
$avg = average();
system "echo 23.1 $avg >> $dirname/q_time_t.data";

$raw_data = `grep THRUPUT $infile | grep 'RF2 ' | awk '{print \$11}' | xargs`;
@data = split ' ', $raw_data;
$avg = average();
system "echo 24.1 $avg >> $dirname/q_time_t.data";

my $outfile = new FileHandle;
unless( $outfile->open( "> $dirname/q_time.input" ) ) {
	die "cannot open $dirname/q_time.input $!";
}

print $outfile "set style line 1 lt 1 lw 50\n";
print $outfile "set style line 2 lt 2 lw 50\n";
print $outfile "plot \"q_time_p.data\" using 1:2 title \"Power\" with imp ls 1, \"q_time_t.data\" using 1:2 title \"Throughput\" with imp ls 2\n";
print $outfile "set term png small\n";
print $outfile "set output \"q_time.png\"\n";
print $outfile "set xtics \(\"Q1\" 1, \"Q2\" 2, \"Q3\" 3, \"Q4\" 4, \"Q5\" 5, \"Q6\" 6, \"Q7\" 7, \"Q8\" 8, \"Q9\" 9, \"Q10\" 10, \"Q11\" 11, \"Q12\" 12, \"Q13\" 13, \"Q14\" 14, \"Q15\" 15, \"Q16\" 16, \"Q17\" 17, \"Q18\" 18, \"Q19\" 19, \"Q20\" 20, \"Q21\" 21, \"Q22\" 22, \"RF1\" 23, \"RF2\" 24\)\n";
print $outfile "set ylabel \"Query Time in Seconds\"\n";
print $outfile "set xrange [0:25]\n";
print $outfile "replot\n";
$outfile->close;

chdir $dirname;
system "gnuplot q_time.input";
