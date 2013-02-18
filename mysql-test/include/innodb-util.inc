#
# Utility functions to copy files for WL#5522
#
# All the tables must be in the same database, you can call it like so:
# ib_backup_tablespaces("test", "t1", "blah", ...).

perl;
use File::Copy;
use File::Spec;

sub ib_normalize_path {
    my ($path) = @_;
}

sub ib_backup_tablespace {
    my ($db, $table) = @_;
    my $datadir = $ENV{'MYSQLD_DATADIR'};
    my $cfg_file = sprintf("%s.cfg", $table);
    my $ibd_file = sprintf("%s.ibd", $table);
    my $tmpd = $ENV{'MYSQLTEST_VARDIR'} . "/tmp";

    my @args = (File::Spec->catfile($datadir, $db, $ibd_file),
		File::Spec->catfile($tmpd, $ibd_file));

    copy(@args) or die "copy @args failed: $!";

    my @args = (File::Spec->catfile($datadir, $db, $cfg_file),
	        File::Spec->catfile($tmpd, $cfg_file));

    copy(@args) or die "copy @args failed: $!";
}

sub ib_cleanup {
    my ($db, $table) = @_;
    my $datadir = $ENV{'MYSQLD_DATADIR'};
    my $cfg_file = sprintf("%s.cfg", $table);

    print "unlink: $cfg_file\n";

    # These may or may not exist
    unlink(File::Spec->catfile($datadir, $db, $cfg_file));
}

sub ib_unlink_tablespace {
    my ($db, $table) = @_;
    my $datadir = $ENV{'MYSQLD_DATADIR'};
    my $ibd_file = sprintf("%s.ibd", $table);

    print "unlink: $ibd_file\n";
    # This may or may not exist
    unlink(File::Spec->catfile($datadir, $db, $ibd_file));

    ib_cleanup($db, $table);
}

sub ib_backup_tablespaces {
    my ($db, @tables) = @_;

    foreach my $table (@tables) {
        print "backup: $table\n";
        ib_backup_tablespace($db, $table);
    }
}

sub ib_discard_tablespace { }

sub ib_discard_tablespaces { }

sub ib_restore_cfg_file {
    my ($tmpd, $datadir, $db, $table) = @_;
    my $cfg_file = sprintf("%s.cfg", $table);

    my @args = (File::Spec->catfile($tmpd, $cfg_file),
		File::Spec->catfile($datadir, "$db", $cfg_file));

    copy(@args) or die "copy @args failed: $!";
}

sub ib_restore_ibd_file {
    my ($tmpd, $datadir, $db, $table) = @_;
    my $ibd_file = sprintf("%s.ibd", $table);

    my @args = (File::Spec->catfile($tmpd, $ibd_file),
		File::Spec->catfile($datadir, $db, $ibd_file));

    copy(@args) or die "copy @args failed: $!";
}

sub ib_restore_tablespace {
    my ($db, $table) = @_;
    my $datadir = $ENV{'MYSQLD_DATADIR'};
    my $tmpd = $ENV{'MYSQLTEST_VARDIR'} . "/tmp";

    ib_restore_cfg_file($tmpd, $datadir, $db, $table);
    ib_restore_ibd_file($tmpd, $datadir, $db, $table);
}

sub ib_restore_tablespaces {
    my ($db, @tables) = @_;

    foreach my $table (@tables) {
        print "restore: $table .ibd and .cfg files\n";
        ib_restore_tablespace($db, $table);
    }
}

sub ib_restore_cfg_files {
    my ($db, @tables) = @_;
    my $datadir = $ENV{'MYSQLD_DATADIR'};
    my $tmpd = $ENV{'MYSQLTEST_VARDIR'} . "/tmp";

    foreach my $table (@tables) {
      print "restore: $table .cfg file\n";
      ib_restore_cfg_file($tmpd, $datadir, $db, $table);
    }
}

sub ib_restore_ibd_files {
    my ($db, @tables) = @_;
    my $datadir = $ENV{'MYSQLD_DATADIR'};
    my $tmpd = $ENV{'MYSQLTEST_VARDIR'} . "/tmp";

    foreach my $table (@tables) {
        print "restore: $table .ibd file\n";
        ib_restore_ibd_file($tmpd, $datadir, $db, $table);
    }
}
EOF
