package My::Suite::Plugins;

@ISA = qw(My::Suite);

$ENV{PAM_SETUP_FOR_MTR}=1 if -e '/etc/pam.d/mariadb_mtr';

bless { };

