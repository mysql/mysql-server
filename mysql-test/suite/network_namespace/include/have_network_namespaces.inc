# The inc file ensures that the the test which it sources
# runs on linux and that the programs 'mysqld', 'mysql' and
# 'mysqlxtest' have the 'cap_sys_admin' capability granted.
# It also checks whether a couple of network namespaces
# named 'red' and 'blue' are configured on the machine.

--perl
  use strict;
  use File::Basename;

  my $osname = $^O;
  my $mysqld_cap_set = 0;
  my $mysql_cap_set = 0;
  my $mysqlxtest_cap_set = 0;
  my $red_netns_set = 0;
  my $blue_netns_set = 0;
  my $platform_support = 0;
  my $exe_mysqlxtest;

  if( $osname eq 'linux')
  {
    $platform_support = 1;

    `/sbin/getcap $ENV{'MYSQLD'} | grep -F -q cap_sys_admin`;
    $mysqld_cap_set = 1 if $? == 0;

    `/sbin/getcap $ENV{'EXE_MYSQL'} | grep -F -q cap_sys_admin`;
    $mysql_cap_set = 1 if $? == 0;

    ($exe_mysqlxtest) = split (/\s/, $ENV{'MYSQLXTEST'});
    `/sbin/getcap $exe_mysqlxtest | grep -F -q cap_sys_admin`;
    $mysqlxtest_cap_set = 1 if $? == 0;

    `/sbin/ip netns list | grep -F -q red`;
    $red_netns_set = 1 if $? == 0;

    `/sbin/ip netns list | grep -F -q blue`;
    $blue_netns_set = 1 if $? == 0;
  }

  open(FH, ">$ENV{MYSQL_TMP_DIR}/netns_vars.inc") or
    die("Unable to open file: $!\n");
  print FH "let \$mysqld_cap_set= $mysqld_cap_set;\n";
  print FH "let \$mysql_cap_set= $mysql_cap_set;\n";
  print FH "let \$mysqlxtest_cap_set= $mysqlxtest_cap_set;\n";
  print FH "let \$exe_mysqlxtest= $exe_mysqlxtest;\n";
  print FH "let \$red_netns_set= $red_netns_set;\n";
  print FH "let \$blue_netns_set= $blue_netns_set;\n";
  close FH;
EOF

--source $MYSQL_TMP_DIR/netns_vars.inc
--remove_file $MYSQL_TMP_DIR/netns_vars.inc

if ($platform_support == 0)
{
  --skip Test is not supported on the platform
}

if ($mysqld_cap_set == 0)
{
  --skip Test requires that mysqld has cap_sys_admin
}

if ($mysql_cap_set == 0)
{
  --skip Test requires that mysql client has cap_sys_admin
}

if ($mysqlxtest_cap_set == 0)
{
  --skip Test requires that mysqlxtest client has cap_sys_admin
}

if ($red_netns_set == 0)
{
  --skip Test requires that network namespace 'red' is present
}

if ($blue_netns_set == 0)
{
  --skip Test requires that network namespace 'blue' is present
}
