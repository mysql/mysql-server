#
# Uses /etc/os-release to set variable $linux_distro
# Examples: fedora, debian, ubuntu, ol, opensuse-leap, sles, rhel, alpine
#

if (`select convert(@@version_compile_os using latin1) IN ("Linux") = 0`)
{
  let $linux_distro = unknown;
}

if (`select convert(@@version_compile_os using latin1) IN ("Linux") = 1`)
{
  --perl
    use strict;
    use warnings;
    my $os_release= '/etc/os-release';
    my $distro= 'unknown';
    open (my $WH, ">", "$ENV{MYSQL_TMP_DIR}/linux_distro.inc") or die "Could not open $ENV{MYSQL_TMP_DIR}/linux_distro.inc: $!";
    if (-r $os_release) {
      open(my $RH,  "<",  $os_release) or die "Could not open $os_release $!";
      while (<$RH>) {
        if (/^ID=/) {
          (my $dummy, $distro) = split /=/;
          chomp $distro;
        }
      }
      close $RH;
    }
    print $WH "let \$linux_distro = $distro;\n";
    close $WH;
  EOF
  --source $MYSQL_TMP_DIR/linux_distro.inc
  --remove_file $MYSQL_TMP_DIR/linux_distro.inc
}
