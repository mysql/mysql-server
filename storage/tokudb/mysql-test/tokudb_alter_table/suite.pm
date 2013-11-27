package My::Suite::TokuDB_alter_table;
use File::Basename;
@ISA = qw(My::Suite);

#return "Not run for embedded server" if $::opt_embedded_server;
return "No TokuDB engine" unless $ENV{HA_TOKUDB_SO} or $::mysqld_variables{tokudb};

sub is_default { 1 }

bless { };

