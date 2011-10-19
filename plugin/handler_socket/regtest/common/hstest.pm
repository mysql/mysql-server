
# vim:sw=2:ai

package hstest;

use DBI;
use Net::HandlerSocket;

our %conf = ();

sub get_conf_env {
  my ($key, $defval) = @_;
  return $ENV{$key} || $defval;
}

sub init_conf {
  $conf{host} = get_conf_env("MYHOST", "localhost");
  $conf{myport} = get_conf_env("MYPORT", 3306);
  $conf{dbname} = get_conf_env("MYDBNAME", "hstestdb");
  $conf{ssps} = get_conf_env("MYSSPS");
  $conf{user} = get_conf_env("MYSQLUSER", "root");
  $conf{pass} = get_conf_env("MYSQLPASS", "");
  $conf{hsport} = get_conf_env("HSPORT", 9998);
  $conf{hspass} = get_conf_env("HSPASS", undef);
}

sub get_dbi_connection {
  my ($dbname, $host, $myport, $ssps, $user, $pass)
    = ($conf{dbname}, $conf{host}, $conf{myport}, $conf{ssps},
      $conf{user}, $conf{pass});
  my $mycnf = "binary_my.cnf";
  my $dsn = "DBI:mysql:database=;host=$host;port=$myport"
    . ";mysql_server_prepare=$ssps"
    . ";mysql_read_default_group=perl"
    . ";mysql_read_default_file=../common/$mycnf";
  my $dbh = DBI->connect($dsn, $user, $pass, { RaiseError => 1 });
  return $dbh;
}

sub init_testdb {
  my $charset = $_[0] || "binary";
  my $dbh = get_dbi_connection();
  my $dbname = $conf{dbname};
  $dbh->do("drop database if exists $dbname");
  $dbh->do("create database $dbname default character set $charset");
  $dbh->do("use $dbname");
  return $dbh;
}

sub get_hs_connection {
  my ($host, $port) = @_;
  $host ||= $conf{host};
  $port ||= $conf{hsport};
  my $hsargs = { 'host' => $host, 'port' => $port };
  my $conn = new Net::HandlerSocket($hsargs);
  if (defined($conn) && defined($conf{hspass})) {
    $conn->auth($conf{hspass});
  }
  return $conn;
}


init_conf();

1;

