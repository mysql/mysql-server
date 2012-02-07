package My::Suite::Federated;

@ISA = qw(My::Suite);

sub skip_combinations {
  my @combinations;

  push @combinations, 'old'
     unless $ENV{HA_FEDERATED_SO} and not $::mysqld_variables{'federated'};
  push @combinations, 'X'
     unless $ENV{HA_FEDERATEDX_SO} or $::mysqld_variables{'federated'};

  ( 'combinations' => [ @combinations ] )
}


############# return an object ######################
bless { };

