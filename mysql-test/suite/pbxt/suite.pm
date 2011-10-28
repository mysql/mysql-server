package My::Suite::PBXT;
@ISA = qw(My::Suite);
return "Not run for embedded server" if $::opt_embedded_server;
return "No PBXT engine" unless $ENV{HA_PBXT_SO} or $::mysqld_variables{pbxt};
bless { };
