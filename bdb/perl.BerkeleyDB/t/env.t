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
use File::Path qw(rmtree);

print "1..52\n";


{
    package LexFile ;

    sub new
    {
        my $self = shift ;
        unlink @_ ;
        bless [ @_ ], $self ;
    }

    sub DESTROY
    {
        my $self = shift ;
        unlink @{ $self } ;
    }
}

sub ok
{
    my $no = shift ;
    my $result = shift ;
 
    print "not " unless $result ;
    print "ok $no\n" ;
}

sub docat
{
    my $file = shift;
    local $/ = undef;
    open(CAT,$file) || die "Cannot open $file:$!";
    my $result = <CAT>;
    close(CAT);
    return $result;
}


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
    ok 11, -d $home ? chmod 0777, $home : mkdir($home, 0777) ;
    mkdir "./fred", 0777 ;
    chdir "./fred" ;
    ok 12, my $env = new BerkeleyDB::Env -Flags => DB_CREATE ;
    chdir ".." ;
    undef $env ;
    rmtree $home ;
}

{
    # create an environment with a Home
    my $home = "./fred" ;
    ok 13, -d $home ? chmod 0777, $home : mkdir($home, 0777) ;
    ok 14, my $env = new BerkeleyDB::Env -Home => $home,
    					 -Flags => DB_CREATE ;

    undef $env ;
    rmtree $home ;
}

{
    # make new fail.
    my $home = "./not_there" ;
    rmtree $home ;
    ok 15, ! -d $home ;
    my $env = new BerkeleyDB::Env -Home => $home,
			          -Flags => DB_INIT_LOCK ;
    ok 16, ! $env ;
    ok 17,   $! != 0 ;

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
    ok 18, -d $home ? chmod 0777, $home : mkdir($home, 0777) ;
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
    rmtree $home ;
}

{
    # -ErrFile with a filename
    my $errfile = "./errfile" ;
    my $home = "./fred" ;
    ok 24, -d $home ? chmod 0777, $home : mkdir($home, 0777) ;
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
    rmtree $home ;
}

{
    # -ErrFile with a filehandle
    use IO ;
    my $home = "./fred" ;
    ok 30, -d $home ? chmod 0777, $home : mkdir($home, 0777) ;
    my $errfile = "./errfile" ;
    my $lex = new LexFile $errfile ;
    ok 31, my $ef  = new IO::File ">$errfile" ;
    ok 32, my $env = new BerkeleyDB::Env( -ErrFile => $ef ,
    					  -Flags => DB_CREATE,
					  -Home   => $home) ;
    my $db = new BerkeleyDB::Hash -Filename => $Dfile,
			     -Env      => $env,
			     -Flags    => -1;
    ok 33, !$db ;

    ok 34, $BerkeleyDB::Error =~ /^illegal flag specified to (db_open|DB->open)/;
    $ef->close() ;
    ok 35, -e $errfile ;
    my $contents = "" ;
    $contents = docat($errfile) ;
    chomp $contents ;
    ok 36, $BerkeleyDB::Error eq $contents ;
    undef $env ;
    rmtree $home ;
}

{
    # -ErrPrefix
    use IO ;
    my $home = "./fred" ;
    ok 37, -d $home ? chmod 0777, $home : mkdir($home, 0777) ;
    my $errfile = "./errfile" ;
    my $lex = new LexFile $errfile ;
    ok 38, my $env = new BerkeleyDB::Env( -ErrFile => $errfile,
					-ErrPrefix => "PREFIX",
    					  -Flags => DB_CREATE,
					  -Home   => $home) ;
    my $db = new BerkeleyDB::Hash -Filename => $Dfile,
			     -Env      => $env,
			     -Flags    => -1;
    ok 39, !$db ;

    ok 40, $BerkeleyDB::Error =~ /^PREFIX: illegal flag specified to (db_open|DB->open)/;
    ok 41, -e $errfile ;
    my $contents = docat($errfile) ;
    chomp $contents ;
    ok 42, $BerkeleyDB::Error eq $contents ;

    # change the prefix on the fly
    my $old = $env->errPrefix("NEW ONE") ;
    ok 43, $old eq "PREFIX" ;

    $db = new BerkeleyDB::Hash -Filename => $Dfile,
			     -Env      => $env,
			     -Flags    => -1;
    ok 44, !$db ;
    ok 45, $BerkeleyDB::Error =~ /^NEW ONE: illegal flag specified to (db_open|DB->open)/;
    $contents = docat($errfile) ;
    chomp $contents ;
    ok 46, $contents =~ /$BerkeleyDB::Error$/ ;
    undef $env ;
    rmtree $home ;
}

{
    # test db_appexit
    use Cwd ;
    my $cwd = cwd() ;
    my $home = "$cwd/fred" ;
    my $data_dir = "$home/data_dir" ;
    my $log_dir = "$home/log_dir" ;
    my $data_file = "data.db" ;
    ok 47, -d $home ? chmod 0777, $home : mkdir($home, 0777) ;
    ok 48, -d $data_dir ? chmod 0777, $data_dir : mkdir($data_dir, 0777) ;
    ok 49, -d $log_dir ? chmod 0777, $log_dir : mkdir($log_dir, 0777) ;
    my $env = new BerkeleyDB::Env -Home   => $home,
			      -Config => { DB_DATA_DIR => $data_dir,
					   DB_LOG_DIR  => $log_dir
					 },
			      -Flags  => DB_CREATE|DB_INIT_TXN|DB_INIT_LOG|
					 DB_INIT_MPOOL|DB_INIT_LOCK ;
    ok 50, $env ;

    ok 51, my $txn_mgr = $env->TxnMgr() ;

    ok 52, $env->db_appexit() == 0 ;

    #rmtree $home ;
}

# test -Verbose
# test -Flags
# db_value_set
