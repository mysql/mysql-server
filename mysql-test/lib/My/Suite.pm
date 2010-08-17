# A default suite class that is used for all suites without their owns suite.pm
# see README.suites for a description

package My::Suite;

sub config_files { () }
sub servers { () }

bless { };

