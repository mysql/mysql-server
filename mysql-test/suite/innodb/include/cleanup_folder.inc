# Remove residue files from data folder, of page size 32K
perl;
use File::Path;
my $tempdir= $ENV{'MYSQL_TMP_DIR'}."/datadir1/data";
die "usage: $0 dir ..\n" unless $tempdir;
    foreach $dir ($tempdir) {
    rmtree($dir);
}
EOF
