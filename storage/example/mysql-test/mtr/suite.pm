package My::Suite::MTR::Example;

@ISA = qw(My::Suite);

sub skip_combinations {(
    't/combs.combinations' => [ 'c1' ],
)}
bless { };
