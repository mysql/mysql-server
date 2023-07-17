# If supplied, use a custom path for a 64bit Perl.
--let PATH = $MTR_PERL64_PATH:$PATH

perl;
use Config;
my $result_file_path = "$ENV{'MYSQLTEST_VARDIR'}/have_perl_64bit_int-result.inc";
open my $fh, '+>', $result_file_path or die "open failed: $!\n";
my $has_64bit_int = $Config{use64bitint} == "define";
my $msg = "let PERL_64BIT_INT=$has_64bit_int;";
syswrite($fh, "$msg\n") > 0 or die "write failed: $!\n";
close $fh or die "close failed: $!\n";
EOF

--source $MYSQLTEST_VARDIR/have_perl_64bit_int-result.inc
--remove_file $MYSQLTEST_VARDIR/have_perl_64bit_int-result.inc
if (!$PERL_64BIT_INT) {
    --skip The perl is compiled without support for 64bit integers.
}
