# -*- cperl -*-

# This is a library file used by the Perl version of mysql-test-run,
# and is part of the translation of the Bourne shell script with the
# same name.

use strict;

# These are not to be prefixed with "mtr_"

sub gprof_prepare ();
sub gprof_collect ();

##############################################################################
#
#  
#
##############################################################################

sub gprof_prepare () {

  rmtree($::opt_gprof_dir);
  mkdir($::opt_gprof_dir);
}

# FIXME what about master1 and slave1?!
sub gprof_collect () {

  if ( -f "$::master->[0]->{'path_myddir'}/gmon.out" )
  {
    # FIXME check result code?!
    mtr_run("gprof",
            [$::exe_master_mysqld,
             "$::master->[0]->{'path_myddir'}/gmon.out"],
            $::opt_gprof_master, "", "", "");
    print "Master execution profile has been saved in $::opt_gprof_master\n";
  }
  if ( -f "$::slave->[0]->{'path_myddir'}/gmon.out" )
  {
    # FIXME check result code?!
    mtr_run("gprof",
            [$::exe_slave_mysqld,
             "$::slave->[0]->{'path_myddir'}/gmon.out"],
            $::opt_gprof_slave, "", "", "");
    print "Slave execution profile has been saved in $::opt_gprof_slave\n";
  }
}


1;
