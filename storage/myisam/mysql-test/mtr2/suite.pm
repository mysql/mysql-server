package My::Suite::MTR2::MyISAM;

@ISA = qw(My::Suite);

sub skip_combinations {(
    'combinations' => [ '1st' ],
)}
bless { };

