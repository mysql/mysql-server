let FILE= $MYSQLTEST_VARDIR/tmp/ibd2sdi-debug;
--exec $IBD2SDI --version > $FILE
perl;
use strict;
open(FILE, $ENV{FILE}) or die;
$_ = <FILE>;
close(FILE);
my $debug = /ibd2sdi.*debug /;
open(FILE, ">$ENV{FILE}") or die;
print FILE "let \$debug= $debug;\n";
close(FILE);
EOF

--source $FILE
--remove_file $FILE

if (!$debug) {
  --skip Test requires ibd2sdi-debug
}
