package My::Suite::InnoDB_plugin;

@ISA = qw(My::Suite);

############# initialization ######################
my @combinations=('none');

push @combinations, 'innodb_plugin' if $ENV{HA_INNODB_PLUGIN_SO};
push @combinations, 'xtradb_plugin' if $ENV{HA_XTRADB_SO};
push @combinations, 'xtradb' if $::mysqld_variables{'innodb'} eq "ON";

$ENV{INNODB_PLUGIN_COMBINATIONS}=join ':', @combinations
  unless $ENV{INNODB_PLUGIN_COMBINATIONS};

############# return an object ######################
bless { };

