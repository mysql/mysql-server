
# See also mysql-test/suite/network_namespace/include/have_network_namespaces.inc

# The inc file ensures that the test which it sources
# runs on linux and that the programs 'mysqld'
# have the 'cap_sys_admin' capability granted.
# It also checks whether a couple of network namespaces
# named 'red' and 'blue' are configured on the machine.

--perl
  use strict;
  use File::Basename;

  my $osname = $^O;
  my $mysqld_cap_set = 0;
  my $red_netns_set = 0;
  my $blue_netns_set = 0;
  my $red_netns_file_set = 0;
  my $blue_netns_file_set = 0;
  my $platform_support = 0;

  if( $osname eq 'linux')
  {
    $platform_support = 1;

    `/sbin/getcap $ENV{'MYSQLD'} | grep -F -q cap_sys_admin`;
    $mysqld_cap_set = 1 if $? == 0;

    `/sbin/ip netns list | grep -F -q red`;
    $red_netns_set = 1 if $? == 0;

    `/sbin/ip netns list | grep -F -q blue`;
    $blue_netns_set = 1 if $? == 0;

    $red_netns_file_set = 1 if (-e "/run/netns/red");

    $blue_netns_file_set = 1 if (-e "/run/netns/blue");
  }

  open(FH, ">$ENV{MYSQL_TMP_DIR}/netns_vars.inc") or
    die("Unable to open file: $!\n");
  print FH "let \$mysqld_cap_set= $mysqld_cap_set;\n";
  print FH "let \$red_netns_set= $red_netns_set;\n";
  print FH "let \$blue_netns_set= $blue_netns_set;\n";
  print FH "let \$red_netns_file_set= $red_netns_file_set;\n";
  print FH "let \$blue_netns_file_set= $blue_netns_file_set;\n";
  close FH;
EOF

--source $MYSQL_TMP_DIR/netns_vars.inc
--remove_file $MYSQL_TMP_DIR/netns_vars.inc

if ($platform_support == 0)
{
  --skip Test is not supported on the platform
}

--echo # Ok, network namespace platform support

if ($mysqld_cap_set == 0)
{
  --skip Test requires that mysqld has cap_sys_admin
}

--echo # Ok, mysqld has cap_sys_admin

if ($red_netns_set == 0)
{
  --skip Test requires that network namespace 'red' is present
}

--echo # Ok, red network exists

if ($blue_netns_set == 0)
{
  --skip Test requires that network namespace 'blue' is present
}

--echo # Ok, blue network exists

if ($red_netns_file_set == 0)
{
  --skip Test requires that network namespace file '/run/netns/red' is present
}

--echo # Ok, /run/netns/red file exists

if ($blue_netns_file_set == 0)
{
  --skip Test requires that network namespace file '/run/netns/blue' is present
}

--echo # Ok, /run/netns/blue file exists

--echo # All network namespace pre requisites checked
