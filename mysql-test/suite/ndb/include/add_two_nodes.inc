--perl

my $vardir = $ENV{MYSQLTEST_VARDIR} or die "Need MYSQLTEST_VARDIR";
my $file ="$vardir/my.cnf";
my $file_new = "$vardir/my.cnf.new";

open (IN, "$file") || die $!;
open (OUT, ">$file_new") || die $!;

while ($_ = <IN> ) {
  if ($_ =~ /ndbd=localhost,localhost/i) 
  {
    # Replace text, all instances on a line (/g), case insensitive (/i)
    $_ =~ s/ndbd=localhost,localhost/ndbd=localhost,localhost,localhost,localhost/gi;
  }
  print OUT "$_";
  if ($_=~ /cluster_config.ndb_mgmd.1.1/i) 
  {
    print OUT "NodeId=3\n";
  }
}

close IN;
close OUT;

open (OUT, ">>$file_new") || die $!;
print OUT "[cluster_config.ndbd.3.1]\n";
print OUT "Id=40\n";
print OUT "DataDir=$vardir/mysql_cluster.1/ndbd.1\n";
print OUT "BackupDataDir=$vardir/mysql_cluster.1/ndbd.1/uf\n";
print OUT "FileSystemPathDataFiles=$vardir/mysql_cluster.1/ndbd.1/uf\n";
print OUT "\n";
print OUT "[cluster_config.ndbd.4.1]\n";
print OUT "Id=41\n";
print OUT "DataDir=$vardir/mysql_cluster.1/ndbd.2\n";
print OUT "BackupDataDir=$vardir/mysql_cluster.1/ndbd.2/uf\n";
print OUT "FileSystemPathDataFiles=$vardir/mysql_cluster.1/ndbd.2/uf\n";
close OUT;
EOF