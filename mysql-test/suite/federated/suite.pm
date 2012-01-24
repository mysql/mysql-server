package My::Suite::Federated;

@ISA = qw(My::Suite);

############# initialization ######################
my @combinations;

push @combinations, 'old'
   if $ENV{HA_FEDERATED_SO} and not $::mysqld_variables{'federated'};
push @combinations, 'X'
   if $ENV{HA_FEDERATEDX_SO} or $::mysqld_variables{'federated'};

return "Neither Federated nor FederatedX are available" unless @combinations;

$ENV{FEDERATED_COMBINATIONS}=join ':', @combinations
  unless $ENV{FEDERATED_COMBINATIONS};

############# return an object ######################
bless { };

