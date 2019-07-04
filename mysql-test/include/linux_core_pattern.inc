# Unless /proc/sys/kernel/core_pattern is core and core uses pid is enabled skip test

--source 'include/linux.inc'

--perl
  use strict;
  use warnings;
  my $core = "/proc/sys/kernel/core_pattern";
  my $pid = "/proc/sys/kernel/core_uses_pid";
  open (my $WH, ">", "$ENV{MYSQL_TMP_DIR}/core_pattern.inc") or die "Could not open $ENV{MYSQL_TMP_DIR}/core_pattern.inc: $!";
  if (-r $core && -r $pid) {
    open (my $RH, "<", $core) or die "Can't open $core: $!";
    open (my $PH, "<", $pid) or die "Can't open $pid: $!";
    my $val = <$RH>;
    chomp($val);
    my $uses_pid = <$PH>;
    chomp($uses_pid);
    if ($val eq "core" && $uses_pid eq "1") {
      print $WH "let \$skip = 0;";
    } else {
      print $WH "let \$skip = 1;";
    }
    close $RH;
    close $PH;
  } else {
    print $WH "let \$skip = 1;";
  }
  close $WH;
EOF

--source $MYSQL_TMP_DIR/core_pattern.inc
--remove_file $MYSQL_TMP_DIR/core_pattern.inc

if ($skip) {
  --skip Core pattern is not 'core', uses pid is disabled or /proc/sys/kernel/core_{pattern,uses_pid} not readable
}
