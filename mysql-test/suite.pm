package My::Suite::Main;
use My::Platform;

@ISA = qw(My::Suite);

sub skip_combinations {
  my @combinations;

  # disable innodb/xtradb combinatons for configurations that were not built
  push @combinations, 'innodb_plugin' unless $ENV{HA_INNODB_SO};
  push @combinations, 'xtradb_plugin' unless $ENV{HA_XTRADB_SO};
  push @combinations, 'xtradb' unless $::mysqld_variables{'innodb'} eq "ON";

  my %skip = ( 'include/have_innodb.combinations' => [ @combinations ],
               'include/have_xtradb.combinations' => [ @combinations ]);

  # don't run tests for the wrong platform
  $skip{'include/platform.combinations'} = [ (IS_WINDOWS) ? 'unix' : 'win' ];

  # as a special case, disable certain include files as a whole
  $skip{'include/not_embedded.inc'} = 'Not run for embedded server'
             if $::opt_embedded_server;

  $skip{'include/have_debug.inc'} = 'Requires debug build'
             unless defined $::mysqld_variables{'debug-dbug'};

  $skip{'include/not_windows.inc'} = 'Requires not Windows' if IS_WINDOWS;

  $skip{'t/plugin_loaderr.test'} = 'needs compiled-in innodb'
            unless $::mysqld_variables{'innodb'} eq "ON";

  # disable tests that use ipv6, if unsupported
  sub ipv6_ok() {
    use Socket;
    return 0 unless socket my $sock, PF_INET6, SOCK_STREAM, getprotobyname('tcp');
    # eval{}, if there's no Socket::sockaddr_in6 at all, old Perl installation
    eval { connect $sock, sockaddr_in6(7, Socket::IN6ADDR_LOOPBACK) };
    return $! != 101;
  }
  $skip{'include/check_ipv6.inc'} = 'No IPv6' unless ipv6_ok();

  $skip{'t/openssl_6975.test'} = 'no or too old openssl'
    unless ! IS_WINDOWS and ! system "openssl ciphers TLSv1.2 2>&1 >/dev/null";

  %skip;
}

bless { };

