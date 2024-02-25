#################### Perl module starts ####################
perl;
my $d_dir = $ENV{'datadir'};
my $s_pattern = $ENV{'search_pattern'};
my $filename = $ENV{'ts_name'};
my $ts_filename="$d_dir/$filename";
open TS_FILE, "<$ts_filename" or die "$!";
@lines = <TS_FILE>;
close TS_FILE;

my $vardir = $ENV{'MYSQLTEST_VARDIR'};
my $filename="$vardir/tmpfile.txt";
open(OP_FILE, '>', $filename) or die "$!";

my $found = 0;
foreach $line (@lines) {
        chomp( $line );
        print OP_FILE;
        if($line =~ /$s_pattern/) {
                $found=1;
        }
}

if ($found == 1) {
        print OP_FILE "table space is Unencrypted.\n";
} else {
        print OP_FILE "table space is Encrypted.\n";
}

close OP_FILE;
EOF
#################### Perl module ends ####################

--echo # Print result
cat_file $MYSQLTEST_VARDIR/tmpfile.txt;

