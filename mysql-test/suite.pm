package My::Suite::Main;

@ISA = qw(My::Suite);

sub skip_combinations {
  my @combinations;

  push @combinations, 'innodb_plugin' unless $ENV{HA_INNODB_SO};
  push @combinations, 'xtradb_plugin' unless $ENV{HA_XTRADB_SO};
  push @combinations, 'xtradb' unless $::mysqld_variables{'innodb'} eq "ON";

  ( 'include/have_innodb.combinations' => [ @combinations ] )
}

bless { };

