package My::Suite::Archive;

@ISA = qw(My::Suite);

return ("Need Archive engine" unless $ENV{HA_ARCHIVE_SO} or
        $::mysqld_variables{'archive'} eq "ON");

bless { };

