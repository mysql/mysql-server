package My::Suite::InnoDB_plugin;

@ISA = qw(My::Suite);

############# initialization ######################
my @combinations;

push @combinations, 'innodb_plugin' if $ENV{HA_INNODB_PLUGIN_SO};
push @combinations, 'xtradb_plugin' if $ENV{HA_XTRADB_SO};
push @combinations, 'xtradb' if $::mysqld_variables{'innodb'} eq "ON";

return "Neither innodb_plugin nor xtradb are available" unless @combinations;

$ENV{INNODB_PLUGIN_COMBINATIONS}=join ':', @combinations
  unless $ENV{INNODB_PLUGIN_COMBINATIONS};

############# return an object ######################
bless { };

