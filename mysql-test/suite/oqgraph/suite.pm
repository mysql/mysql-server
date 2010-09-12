package My::Suite::OQGraph;

@ISA = qw(My::Suite);

return "No OQGraph" unless $ENV{OQGRAPH_ENGINE_SO};

bless { };

