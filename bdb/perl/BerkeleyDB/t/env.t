#!./perl -w

# ID: 1.2, 7/17/97

use strict ;

BEGIN {
    unless(grep /blib/, @INC) {
        chdir 't' if -d 't';
        @INC = '../lib' if -d '../lib';
    }
}

use BerkeleyDB; 
use t::util ;

print "1..47\n";

my $Dfile = "dbhash.tmp";

umask(0);

{
    # db version stuff
    my ($major, $minor, $patch) = (0, 0, 0) ;

    ok 1, my $VER = BerkeleyDB::DB_VERSION_STRING ;
    ok 2, my $ver = BerkeleyDB::db_version($major, $minor, $patch) ;
    ok 3, $VER eq $ver ;
    ok 4, $major > 1 ;
    ok 5, defined $minor ;
    ok 6, defined $patch ;
}

{
    # Check for invalid parameters
    my $env ;
    eval ' $env = new BerkeleyDB::Env( -Stupid => 3) ; ' ;
    ok 7, $@ =~ /unknown key value\(s\) Stupid/  ;

    eval ' $env = new BerkeleyDB::Env( -Bad => 2, -Home => "/tmp", -Stupid => 3) ; ' ;
    ok 8, $@ =~ /unknown key value\(s\) (Bad |Stupid ){2}/  ;

    eval ' $env = new BerkeleyDB::Env (-Config => {"fred" => " "} ) ; ' ;
    ok 9, !$env ;
    ok 10, $BerkeleyDB::Error =~ /^illegal name-value pair/ ;
}

{
    # create a very simple environment
    my $home = "./fred" ;
    ok 11, my $lexD = new LexDir($home) ;
    chdir "./fred" ;
    ok 12, my $env = new BerkeleyDB::Env -Flags => DB_CREATE ;
    chdir ".." ;
    undef $env ;
}

{
    # create an environment with a Home
    my $home = "./fred" ;
    ok 13, my $lexD = new LexDir($home) ;
    ok 14, my $env = new BerkeleyDB::Env -Home => $home,
    					 -Flags => DB_CREATE ;

    undef $env ;
}

{
    # make new fail.
    my $home = "./not_there" ;
    rmtree $home ;
    ok 15, ! -d $home ;
    my $env = new BerkeleyDB::Env -Home => $home,
			          -Flags => DB_INIT_LOCK ;
    ok 16, ! $env ;
    ok 17,   $! != 0 || $^E != 0 ;

    rmtree $home ;
}

{
    # Config
    use Cwd ;
    my $cwd = cwd() ;
    my $home = "$cwd/fred" ;
    my $data_dir = "$home/data_dir" ;
    my $log_dir = "$home/log_dir" ;
    my $data_file = "data.db" ;
    ok 18, my $lexD = new LexDir($home) ;
    ok 19, -d $data_dir ? chmod 0777, $data_dir : mkdir($data_dir, 0777) ;
    ok 20, -d $log_dir ? chmod 0777, $log_dir : mkdir($log_dir, 0777) ;
    my $env = new BerkeleyDB::Env -Home   => $home,
			      -Config => { DB_DATA_DIR => $data_dir,
					   DB_LOG_DIR  => $log_dir
					 },
			      -Flags  => DB_CREATE|DB_INIT_TXN|DB_INIT_LOG|
					 DB_INIT_MPOOL|DB_INIT_LOCK ;
    ok 21, $env ;

    ok 22, my $txn = $env->txn_begin() ;

    my %hash ;
    ok 23, tie %hash, 'BerkeleyDB::Hash', -Filename => $data_file,
                                       -Flags     => DB_CREATE ,
                                       -Env       => $env,
                                       -Txn       => $txn  ;

    $hash{"abc"} = 123 ;
    $hash{"def"} = 456 ;

    $txn->txn_commit() ;

    untie %hash ;

    undef $txn ;
    undef $env ;
}

{
    # -ErrFile with a filename
    my $errfile = "./errfile" ;
    my $home = "./fred" ;
    ok 24, my $lexD = new LexDir($home) ;
    my $lex = new LexFile $errfile ;
    ok 25, my $env = new BerkeleyDB::Env( -ErrFile => $errfile, 
    					  -Flags => DB_CREATE,
					  -Home   => $home) ;
    my $db = new BerkeleyDB::Hash -Filename => $Dfile,
			     -Env      => $env,
			     -Flags    => -1;
    ok 26, !$db ;

    ok 27, $BerkeleyDB::Error =~ /^illegal flag specified to (db_open|DB->open)/;
    ok 28, -e $errfile ;
    my $contents = docat($errfile) ;
    chomp $contents ;
    ok 29, $BerkeleyDB::Error eq $contents ;

    undef $env ;
}

{
    # -ErrFile with a filehandle/reference -- should fail
    my $home = "./fred" ;
    ok 30, my $lexD = new LexDir($home) ;
    eval { my $env = new BerkeleyDB::Env( -ErrFile => [],
    					  -Flags => DB_CREATE,
					  -Home   => $home) ; };
    ok 31, $@ =~ /ErrFile parameter must be a file name/;
}

{
    # -ErrPrefix
    use IO ;
    my $home = "./fred" ;
    ok 32, my $lexD = new LexDir($home) ;
    my $errfile = "./errfile" ;
    my $lex = new LexFile $errfile ;
    ok 33, my $env = new BerkeleyDB::Env( -ErrFile => $errfile,
					-ErrPrefix => "PREFIX",
    					  -Flags => DB_CREATE,
					  -Home   => $home) ;
    my $db = new BerkeleyDB::Hash -Filename => $Dfile,
			     -Env      => $env,
			     -Flags    => -1;
    ok 34, !$db ;

    ok 35, $BerkeleyDB::Error =~ /^PREFIX: illegal flag specified to (db_open|DB->open)/;
    ok 36, -e $errfile ;
    my $contents = docat($errfile) ;
    chomp $contents ;
    ok 37, $BerkeleyDB::Error eq $contents ;

    # change the prefix on the fly
    my $old = $env->errPrefix("NEW ONE") ;
    ok 38, $old eq "PREFIX" ;

    $db = new BerkeleyDB::Hash -Filename => $Dfile,
			     -Env      => $env,
			     -Flags    => -1;
    ok 39, !$db ;
    ok 40, $BerkeleyDB::Error =~ /^NEW ONE: illegal flag specified to (db_open|DB->open)/;
    $contents = docat($errfile) ;
    chomp $contents ;
    ok 41, $contents =~ /$BerkeleyDB::Error$/ ;
    undef $env ;
}

{
    # test db_appexit
    use Cwd ;
    my $cwd = cwd() ;
    my $home = "$cwd/fred" ;
    my $data_dir = "$home/data_dir" ;
    my $log_dir = "$home/log_dir" ;
    my $data_file = "data.db" ;
    ok 42, my $lexD = new LexDir($home);
    ok 43, -d $data_dir ? chmod 0777, $data_dir : mkdir($data_dir, 0777) ;
    ok 44, -d $log_dir ? chmod 0777, $log_dir : mkdir($log_dir, 0777) ;
    my $env = new BerkeleyDB::Env -Home   => $home,
			      -Config => { DB_DATA_DIR => $data_dir,
					   DB_LOG_DIR  => $log_dir
					 },
			      -Flags  => DB_CREATE|DB_INIT_TXN|DB_INIT_LOG|
					 DB_INIT_MPOOL|DB_INIT_LOCK ;
    ok 45, $env ;

    ok 46, my $txn_mgr = $env->TxnMgr() ;

    ok 47, $env->db_appexit() == 0 ;

}

# test -Verbose
# test -Flags
# db_value_set
