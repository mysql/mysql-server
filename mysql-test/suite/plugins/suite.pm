package My::Suite::Plugins;

@ISA = qw(My::Suite);

sub skip_combinations {
  my %skip;
  $skip{'t/pam.test'} = 'No pam setup for mtr'
             unless -e '/etc/pam.d/mariadb_mtr';
  %skip;
}

bless { };

