let $disable_psi_thread = `select count(*) = 0 from performance_schema.threads`;
if ($disable_psi_thread)
{
  skip Test shall be skipped as server is compiled  with DISABLE_PSI_THREAD.;
}

--perl
  use strict;
  my $osname         = $^O;
  my $num_vcpu       = $ENV{NUMBER_OF_CPUS};
  my $thr_prio_enable= 0;

  if( $osname eq 'linux')
  {
    `/sbin/getcap $ENV{'MYSQLD'} | grep -F -q cap_sys_nice`;
    $thr_prio_enable= 1 if $? == 0;
  }

  if ($osname eq 'MSWin32')
  {
     $thr_prio_enable= 1;
  }

  open(FH, ">$ENV{MYSQL_TMP_DIR}/resource_group_vars.inc") or
    die("Unable to open file: $!\n");

  print FH "let \$os_name= $osname;\n";
  print FH "let \$thr_prio_enable= $thr_prio_enable;\n";
  print FH "let \$num_vcpu= $num_vcpu;\n";
  close FH;
EOF

--source $MYSQL_TMP_DIR/resource_group_vars.inc
--remove_file $MYSQL_TMP_DIR/resource_group_vars.inc

if ($os_name == 'darwin')
{
  --skip Test is not applicable on Mac OS
}

if ($num_vcpu < 4)
{
  --skip Test requires atlest 4 VCPUS
}
