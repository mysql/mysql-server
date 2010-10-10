package My::Suite::OQGraph;

@ISA = qw(My::Suite);

return "No OQGraph" unless $ENV{HA_OQGRAPH_SO};

bless { };

