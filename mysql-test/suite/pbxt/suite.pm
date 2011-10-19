package My::Suite::PBXT;
@ISA = qw(My::Suite);
return "No PBXT engine" unless $ENV{HA_PBXT_SO} or $::mysqld_variables{pbxt};
bless { };
