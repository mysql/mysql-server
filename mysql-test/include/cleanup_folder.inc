perl;
use File::Path;
my $tempdir= $ENV{'CLEANUP_FOLDER'};
die "usage: $0 dir ..\n" unless $tempdir;
    foreach $dir ($tempdir) {
    rmtree($dir);
}
EOF
