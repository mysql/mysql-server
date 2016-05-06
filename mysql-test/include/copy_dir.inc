# set $SRC $DEST
# Removes the target dir ($DEST)
# This script doesn't work on windows (copytree issue)
if (!$SRC) {
--die "$SRC not set"
}

if (!$DEST) {
--die "$DEST not set"
}

--let CLEANUP_FOLDER=$DEST
--source include/cleanup_folder.inc

perl;
use strict;
use warnings;
use lib "lib";
use My::File::Path qw(copytree);

my $src= $ENV{'SRC'} or die $!;
my $dest= $ENV{'DEST'} or die $!;
copytree("$src", "$dest", "022") or die $!;
EOF
