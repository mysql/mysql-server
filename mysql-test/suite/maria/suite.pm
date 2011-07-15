package My::Suite::Maria;

@ISA = qw(My::Suite);

return "Need Aria engine" unless defined $::mysqld_variables{'aria-recover'};

bless { };

