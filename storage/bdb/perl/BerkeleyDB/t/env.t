#!./perl -w

# ID: 1.2, 7/17/97

use strict ;

BEGIN {
    unless(grep /blib/, @INC) {
        chdir 't' if -d 't';
        @INC = '../lib' if -d '../lib';
    }
}


BEGIN {
    $ENV{LC_ALL} = 'de_DE@euro';
}

use BerkeleyDB; 
use t::util ;

print "1..53\n";

my $Dfile = "dbhash.tmp";

umask(0);

my $version_major  = 0;

{
    # db version stuff
    my ($major, $minor, $patch) = (0, 0, 0) ;

    ok 1, my $VER = BerkeleyDB::DB_VERSION_STRING ;
    ok 2, my $ver = BerkeleyDB::db_version($version_major, $minor, $patch) ;
    ok 3, $VER eq $ver ;
    ok 4, $version_major > 1 ;
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
    ok 10, $BerkeleyDB::Error =~ /^(illegal name-value pair|Invalid argument)/ ;
    #print " $BerkeleyDB::Error\n";
}

{
    # create a very simple environment
    my $home = "./fred" ;
    ok 11, my $lexD = new LexDir($home) ;
    chdir "./fred" ;
    ok 12, my $env = new BerkeleyDB::Env -Flags => DB_CREATE,
					@StdErrFile;
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
    my $env = new BerkeleyDB::Env -Home => $home, @StdErrFile,
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
    my $env = new BerkeleyDB::Env -Home   => $home, @StdErrFile,
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
    # -ErrFile with a filehandle
    use IO::File ;
    my $errfile = "./errfile" ;
    my $home = "./fred" ;
    ok 30, my $lexD = new LexDir($home) ;
    my $lex = new LexFile $errfile ;
    my $fh = new IO::File ">$errfile" ;
    ok 31, my $env = new BerkeleyDB::Env( -ErrFile => $fh, 
    					  -Flags => DB_CREATE,
					  -Home   => $home) ;
    my $db = new BerkeleyDB::Hash -Filename => $Dfile,
			     -Env      => $env,
			     -Flags    => -1;
    ok 32, !$db ;

    ok 33, $BerkeleyDB::Error =~ /^illegal flag specified to (db_open|DB->open)/;
    ok 34, -e $errfile ;
    my $contents = docat($errfile) ;
    chomp $contents ;
    ok 35, $BerkeleyDB::Error eq $contents ;

    undef $env ;
}

{
    # -ErrPrefix
    my $home = "./fred" ;
    ok 36, my $lexD = new LexDir($home) ;
    my $errfile = "./errfile" ;
    my $lex = new LexFile $errfile ;
    ok 37, my $env = new BerkeleyDB::Env( -ErrFile => $errfile,
					-ErrPrefix => "PREFIX",
    					  -Flags => DB_CREATE,
					  -Home   => $home) ;
    my $db = new BerkeleyDB::Hash -Filename => $Dfile,
			     -Env      => $env,
			     -Flags    => -1;
    ok 38, !$db ;

    ok 39, $BerkeleyDB::Error =~ /^PREFIX: illegal flag specified to (db_open|DB->open)/;
    ok 40, -e $errfile ;
    my $contents = docat($errfile) ;
    chomp $contents ;
    ok 41, $BerkeleyDB::Error eq $contents ;

    # change the prefix on the fly
    my $old = $env->errPrefix("NEW ONE") ;
    ok 42, $old eq "PREFIX" ;

    $db = new BerkeleyDB::Hash -Filename => $Dfile,
			     -Env      => $env,
			     -Flags    => -1;
    ok 43, !$db ;
    ok 44, $BerkeleyDB::Error =~ /^NEW ONE: illegal flag specified to (db_open|DB->open)/;
    $contents = docat($errfile) ;
    chomp $contents ;
    ok 45, $contents =~ /$BerkeleyDB::Error$/ ;
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
    ok 46, my $lexD = new LexDir($home);
    ok 47, -d $data_dir ? chmod 0777, $data_dir : mkdir($data_dir, 0777) ;
    ok 48, -d $log_dir ? chmod 0777, $log_dir : mkdir($log_dir, 0777) ;
    my $env = new BerkeleyDB::Env -Home   => $home, @StdErrFile,
			      -Config => { DB_DATA_DIR => $data_dir,
					   DB_LOG_DIR  => $log_dir
					 },
			      -Flags  => DB_CREATE|DB_INIT_TXN|DB_INIT_LOG|
					 DB_INIT_MPOOL|DB_INIT_LOCK ;
    ok 49, $env ;

    ok 50, my $txn_mgr = $env->TxnMgr() ;

    ok 51, $env->db_appexit() == 0 ;

}

{
    # attempt to open a new environment without DB_CREATE
    # should fail with Berkeley DB 3.x or better.

    my $home = "./fred" ;
    ok 52, my $lexD = new LexDir($home) ;
    chdir "./fred" ;
    my $env = new BerkeleyDB::Env -Home => $home, -Flags => DB_CREATE ;
    ok 53, $version_major == 2 ? $env : ! $env ;

    # The test below is not portable -- the error message returned by
    # $BerkeleyDB::Error is locale dependant.

    #ok 54, $version_major == 2 ? 1 
    #                           : $BerkeleyDB::Error =~ /No such file or directory/ ;
    #    or print "# BerkeleyDB::Error is $BerkeleyDB::Error\n";
    chdir ".." ;
    undef $env ;
}

# test -Verbose
# test -Flags
# db_value_set
