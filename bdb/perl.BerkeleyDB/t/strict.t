#!./perl -w

use strict ;

BEGIN {
    unless(grep /blib/, @INC) {
        chdir 't' if -d 't';
        @INC = '../lib' if -d '../lib';
    }
}

use BerkeleyDB; 
use File::Path qw(rmtree);

print "1..44\n";


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
my $home = "./fred" ;

umask(0);

{
    # closing a database & an environment in the correct order.
    my $lex = new LexFile $Dfile ;
    my %hash ;
    my $status ;

    rmtree $home if -e $home ;
    ok 1, mkdir($home, 0777) ;
    ok 2, my $env = new BerkeleyDB::Env -Home => $home,
                                     -Flags => DB_CREATE|DB_INIT_TXN|
                                                DB_INIT_MPOOL|DB_INIT_LOCK ;
					  	
    ok 3, my $db1 = tie %hash, 'BerkeleyDB::Hash', -Filename => $Dfile,
                                      	       	-Flags     => DB_CREATE ,
					       	-Env 	   => $env;

    ok 4, $db1->db_close() == 0 ; 

    eval { $status = $env->db_appexit() ; } ;
    ok 5, $status == 0 ;
    ok 6, $@ eq "" ;
    #print "[$@]\n" ;

    rmtree $home if -e $home ;
}

{
    # closing an environment with an open database
    my $lex = new LexFile $Dfile ;
    my %hash ;

    rmtree $home if -e $home ;
    ok 7, mkdir($home, 0777) ;
    ok 8, my $env = new BerkeleyDB::Env -Home => $home,
                                     -Flags => DB_CREATE|DB_INIT_TXN|
                                                DB_INIT_MPOOL|DB_INIT_LOCK ;
					  	
    ok 9, my $db1 = tie %hash, 'BerkeleyDB::Hash', -Filename => $Dfile,
                                      	       	-Flags     => DB_CREATE ,
					       	-Env 	   => $env;

    eval { $env->db_appexit() ; } ;
    ok 10, $@ =~ /BerkeleyDB Aborting: attempted to close an environment with 1 open database/ ;
    #print "[$@]\n" ;

    undef $db1 ;
    untie %hash ;
    undef $env ;
    rmtree $home if -e $home ;
}

{
    # closing a transaction & a database 
    my $lex = new LexFile $Dfile ;
    my %hash ;
    my $status ;

    rmtree $home if -e $home ;
    ok 11, mkdir($home, 0777) ;
    ok 12, my $env = new BerkeleyDB::Env -Home => $home,
                                     -Flags => DB_CREATE|DB_INIT_TXN|
                                                DB_INIT_MPOOL|DB_INIT_LOCK ;

    ok 13, my $txn = $env->txn_begin() ;
    ok 14, my $db = tie %hash, 'BerkeleyDB::Hash', -Filename => $Dfile,
                                                -Flags     => DB_CREATE ,
					       	-Env 	   => $env,
                                                -Txn       => $txn  ;

    ok 15, $txn->txn_commit()  == 0 ;
    eval { $status = $db->db_close() ; } ;
    ok 16, $status == 0 ;
    ok 17, $@ eq "" ;
    eval { $status = $env->db_appexit() ; } ;
    ok 18, $status == 0 ;
    ok 19, $@ eq "" ;
    #print "[$@]\n" ;
}

{
    # closing a database with an open transaction
    my $lex = new LexFile $Dfile ;
    my %hash ;

    rmtree $home if -e $home ;
    ok 20, mkdir($home, 0777) ;
    ok 21, my $env = new BerkeleyDB::Env -Home => $home,
                                     -Flags => DB_CREATE|DB_INIT_TXN|
                                                DB_INIT_MPOOL|DB_INIT_LOCK ;

    ok 22, my $txn = $env->txn_begin() ;
    ok 23, my $db = tie %hash, 'BerkeleyDB::Hash', -Filename => $Dfile,
                                                -Flags     => DB_CREATE ,
					       	-Env 	   => $env,
                                                -Txn       => $txn  ;

    eval { $db->db_close() ; } ;
    ok 24, $@ =~ /BerkeleyDB Aborting: attempted to close a database while a transaction was still open at/ ;
    #print "[$@]\n" ;
}

{
    # closing a cursor & a database 
    my $lex = new LexFile $Dfile ;
    my %hash ;
    my $status ;
    ok 25, my $db = tie %hash, 'BerkeleyDB::Hash', -Filename => $Dfile,
                                                -Flags     => DB_CREATE ;
    ok 26, my $cursor = $db->db_cursor() ;
    ok 27, $cursor->c_close() == 0 ;
    eval { $status = $db->db_close() ; } ;
    ok 28, $status == 0 ;
    ok 29, $@ eq "" ;
    #print "[$@]\n" ;
    rmtree $home if -e $home ;
}

{
    # closing a database with an open cursor
    my $lex = new LexFile $Dfile ;
    my %hash ;
    ok 30, my $db = tie %hash, 'BerkeleyDB::Hash', -Filename => $Dfile,
                                                -Flags     => DB_CREATE ;
    ok 31, my $cursor = $db->db_cursor() ;
    eval { $db->db_close() ; } ;
    ok 32, $@ =~ /\QBerkeleyDB Aborting: attempted to close a database with 1 open cursor(s) at/;
    #print "[$@]\n" ;
    rmtree $home if -e $home ;
}

{
    # closing a transaction & a cursor 
    my $lex = new LexFile $Dfile ;
    my %hash ;
    my $status ;

    rmtree $home if -e $home ;
    ok 33, mkdir($home, 0777) ;
    ok 34, my $env = new BerkeleyDB::Env -Home => $home,
                                     -Flags => DB_CREATE|DB_INIT_TXN|
                                                DB_INIT_MPOOL|DB_INIT_LOCK ;
    ok 35, my $txn = $env->txn_begin() ;
    ok 36, my $db = tie %hash, 'BerkeleyDB::Hash', -Filename => $Dfile,
                                                -Flags     => DB_CREATE ,
					       	-Env 	   => $env,
                                                -Txn       => $txn  ;
    ok 37, my $cursor = $db->db_cursor() ;
    eval { $status = $cursor->c_close() ; } ;
    ok 38, $status == 0 ;
    ok 39, ($status = $txn->txn_commit())  == 0 ;
    ok 40, $@ eq "" ;
    eval { $status = $db->db_close() ; } ;
    ok 41, $status == 0 ;
    ok 42, $@ eq "" ;
    eval { $status = $env->db_appexit() ; } ;
    ok 43, $status == 0 ;
    ok 44, $@ eq "" ;
    #print "[$@]\n" ;
    rmtree $home if -e $home ;
}

