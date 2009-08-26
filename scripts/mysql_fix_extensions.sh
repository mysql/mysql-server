#!/usr/bin/perl
# This is a utility for MySQL. It is not needed by any standard part
# of MySQL.

# Usage: mysql_fix_extentions datadir
# does not work with RAID, with InnoDB or BDB tables
# makes .frm lowercase and .MYI/MYD/ISM/ISD uppercase
# useful when datafiles are copied from windows

die "Usage: $0 datadir\n" unless -d $ARGV[0];

for $a (<$ARGV[0]/*/*.*>) { $_=$a;
  s/\.frm$/.frm/i;
  s/\.(is[md]|my[id])$/\U$&/i;
  rename ($a, $_) || warn "Cannot rename $a => $_ : $!";
}
