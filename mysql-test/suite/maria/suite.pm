package My::Suite::Maria;

@ISA = qw(My::Suite);

return "Need Aria engine" unless $::mysqld_variables{'aria'} eq "ON";

bless { };

