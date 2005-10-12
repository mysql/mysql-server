use strict;
use IO::Socket;
use DBI;

# mgm info
my $mgmhost = "localhost";
my $mgmport = 38101;

# location of ndb_x_fs
my $datadir = "c2";
my @schemafiles = <$datadir/ndb_*_fs/D[12]/DBDICT/P0.SchemaLog>;
@schemafiles or die "no schemafiles in $datadir";

my $dsn;
$dsn = "dbi:mysql:test:localhost;port=38100";

# this works better for me
my $cnf = $ENV{MYSQL_HOME} . "/var/my.cnf";
$dsn = "dbi:mysql:database=test;host=localhost;mysql_read_default_file=$cnf";

my $dbh;
$dbh = DBI->connect($dsn, 'root', undef, { RaiseError => 0, PrintError => 0 });
$dbh or die $DBI::errstr;

# mgm commands

my $mgm = undef;

sub mgmconnect {
  $mgm = IO::Socket::INET->new(
    Proto => "tcp",
    PeerHost => $mgmhost,
    PeerPort => $mgmport);
  $mgm or die "connect to mgm failed: $!";
  $mgm->autoflush(1);
};

mgmconnect();
warn "connected to mgm $mgmhost $mgmport\n";

my $nodeinfo = {};

sub getnodeinfo {
  $nodeinfo = {};
  $mgm->print("get status\n");
  $mgm->print("\n");
  while (defined($_ = $mgm->getline)) {
    /^node\s+status/ && last;
  }
  while (defined($_ = $mgm->getline)) {
    /^\s*$/ && last;
    /^node\.(\d+)\.(\w+):\s*(\S+)/ && ($nodeinfo->{$1}{$2} = $3);
  }
}

getnodeinfo();

my @dbnode = ();
for my $n (keys %$nodeinfo) {
  my $p = $nodeinfo->{$n};
  ($p->{type} eq 'NDB') && push(@dbnode, $n);
}
@dbnode = sort { $a <=> $b } @dbnode;
@dbnode or die "mgm error, found no db nodes";
warn "db nodes: @dbnode\n";

sub restartnode {
  my($n, $initialstart) = @_;
  warn "restart node $n initialstart=$initialstart\n";
  $mgm->print("restart node\n");
  $mgm->print("node: $n\n");
  $mgm->print("initialstart: $initialstart\n");
  $mgm->print("\n");
  while (1) {
    sleep 5;
    getnodeinfo();
    my $status = $nodeinfo->{$n}{status};
    my $sp = $nodeinfo->{$n}{startphase};
    warn "node $n status: $status sp: $sp\n";
    last if $status eq 'STARTED';
  }
}

sub restartall {
  warn "restart all\n";
  $mgm->print("restart all\n");
  $mgm->print("\n");
  while (1) {
    sleep 5;
    getnodeinfo();
    my $ok = 1;
    for my $n (@dbnode) {
      my $status = $nodeinfo->{$n}{status};
      my $sp = $nodeinfo->{$n}{startphase};
      warn "node $n status: $status sp: $sp\n";
      $ok = 0 if $status ne 'STARTED';
    }
    last if $ok;
  }
}

# the sql stuff

my $maxtab = 300;
my @tab = ();

sub create {
  my($n) = @_;
  my $sql = "create table t$n (a int primary key, b varchar(20), key (b)) engine=ndb";
  warn "create t$n\n";
  $dbh->do($sql) or die "$sql\n$DBI::errstr";
}

sub drop {
  my($n) = @_;
  my $sql = "drop table t$n";
  warn "drop t$n\n";
  $dbh->do($sql) or die "$sql\n$DBI::errstr";
}

sub dropall {
  for my $n (0..($maxtab-1)) {
    my $sql = "drop table if exists t$n";
    $dbh->do($sql) or die "$sql\n$DBI::errstr";
  }
}

sub createdrop {
  my $n = int(rand($maxtab));
  if (! $tab[$n]) {
    create($n);
    $tab[$n] = 1;
  } else {
    drop($n);
    $tab[$n] = 0;
  }
}

sub checkschemafiles {
  system("printSchemaFile -ce @schemafiles");
  $? == 0 or die "schemafiles check failed";
}

sub randomrestart {
  my($k) = @_;
  my $s = int(rand(500));
  if ($s < 2) {
    my $i = $k % scalar(@dbnode);
    my $n = $dbnode[$i];
    my $initialstart = ($s < 1 ? 0 : 1);
    restartnode($n, $initialstart);
    return 1;
  }
  if ($s < 3) {
    restartall();
    return 1;
  }
  return 0;
}

# deterministic
srand(1);

warn "drop any old tables\n";
dropall();

my $loop = 1000000;
for my $k (0..($loop-1)) {
  warn "$k\n";
  createdrop();
  checkschemafiles();
  if (randomrestart($k)) {
    checkschemafiles();
  }
}

$dbh->disconnect or die $DBI::errstr;

# vim: set sw=2:
