# Unless /proc/sys/kernel/core_pattern is core, skip test

--source 'include/linux.inc'

--perl
  use strict;
  use warnings;
  my $core = "/proc/sys/kernel/core_pattern";
  open (my $WH, ">", "$ENV{MYSQL_TMP_DIR}/core_pattern.inc") or die "Could not open $ENV{MYSQL_TMP_DIR}/core_pattern.inc: $!";
  if (-r $core) {
    open (my $RH, "<", $core) or die "Can't open $core: $!";
    my $val = <$RH>;
    chomp($val);
    if ($val eq "core") {
      print $WH "let \$skip = 0;";
    } else {
      print $WH "let \$skip = 1;";
    }
    close $RH;
  } else {
    print $WH "let \$skip = 1;";
  }
  close $WH;
EOF

--source $MYSQL_TMP_DIR/core_pattern.inc
--remove_file $MYSQL_TMP_DIR/core_pattern.inc

if ($skip) {
  --skip Core pattern is not 'core' or /proc/sys/kernel/core_pattern not readable
}
