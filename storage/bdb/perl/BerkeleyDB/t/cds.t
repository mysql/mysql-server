#!./perl -w

# Tests for Concurrent Data Store mode

use strict ;

BEGIN {
    unless(grep /blib/, @INC) {
        chdir 't' if -d 't';
        @INC = '../lib' if -d '../lib';
    }
}

use BerkeleyDB; 
use t::util ;

BEGIN
{
    if ($BerkeleyDB::db_version < 2) {
        print "1..0 # Skip: this needs Berkeley DB 2.x.x or better\n" ;
        exit 0 ;
    }
}



print "1..12\n";

my $Dfile = "dbhash.tmp";
unlink $Dfile;

umask(0) ;

{
    # Error case -- env not opened in CDS mode

    my $lex = new LexFile $Dfile ;

    my $home = "./fred" ;
    ok 1, my $lexD = new LexDir($home) ;

    ok 2, my $env = new BerkeleyDB::Env -Flags => DB_CREATE|DB_INIT_MPOOL,
    					 -Home => $home, @StdErrFile ;

    ok 3, my $db = new BerkeleyDB::Btree -Filename => $Dfile, 
				    -Env      => $env,
				    -Flags    => DB_CREATE ;

    ok 4, ! $env->cds_enabled() ;
    ok 5, ! $db->cds_enabled() ;

    eval { $db->cds_lock() };
    ok 6, $@ =~ /CDS not enabled for this database/;

    undef $db;
    undef $env ;
}

{
    my $lex = new LexFile $Dfile ;

    my $home = "./fred" ;
    ok 7, my $lexD = new LexDir($home) ;

    ok 8, my $env = new BerkeleyDB::Env -Flags => DB_INIT_CDB|DB_CREATE|DB_INIT_MPOOL,
    					 -Home => $home, @StdErrFile ;

    ok 9, my $db = new BerkeleyDB::Btree -Filename => $Dfile, 
				    -Env      => $env,
				    -Flags    => DB_CREATE ;

    ok 10,   $env->cds_enabled() ;
    ok 11,   $db->cds_enabled() ;

    my $cds = $db->cds_lock() ;
    ok 12, $cds ;

    undef $db;
    undef $env ;
}
