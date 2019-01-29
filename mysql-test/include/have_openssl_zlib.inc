#
# Check if openssl binary supports zlib sub command
#
# Sets $have_openssl_zlib to 0|1
#
--error 0,1,2,127
--exec openssl list -cipher-commands > $MYSQL_TMP_DIR/openssl.out 2>&1
--error 0,1,2,127
--exec openssl list-cipher-commands >> $MYSQL_TMP_DIR/openssl.out 2>&1

--perl
  use strict;
  use warnings;
  my $openssl_out= "$ENV{MYSQL_TMP_DIR}/openssl.out";
  my $openssl_zlib= "$ENV{MYSQL_TMP_DIR}/openssl_zlib.inc";
  open (my $RH, "<", $openssl_out) or die "Could not open $openssl_out: $!";
  open (my $WH, ">", $openssl_zlib) or die "Could not open $openssl_zlib: $!";
  my $have_openssl_zlib= 0;
  while (<$RH>) {
    if (/zlib/) {
       $have_openssl_zlib= 1;
       last;
    }
  }
  print $WH "let \$have_openssl_zlib = $have_openssl_zlib;\n";
  close $RH;
  close $WH;
EOF

--source $MYSQL_TMP_DIR/openssl_zlib.inc
--remove_file $MYSQL_TMP_DIR/openssl.out
--remove_file $MYSQL_TMP_DIR/openssl_zlib.inc

if (!$have_openssl_zlib)
{
  --Skip Test requires 'openssl zlib' sub command
}
