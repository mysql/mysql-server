# A default suite class that is used for all suites without their owns suite.pm
# see README.suites for a description

package My::Suite;

sub is_default { 0 }
sub config_files { () }
sub servers { () }
sub skip_combinations { () }

sub new { bless { } }

sub list_cases {
  my ($self, $testdir) = @_;
  opendir(TESTDIR, $testdir) or return ();
  my (@cases) = grep { s/\.test$// } readdir TESTDIR;
  closedir TESTDIR;
  @cases;
}

sub start_test {
  my ($self, $tinfo)= @_;
  &::start_mysqltest($tinfo);
}

bless { };

