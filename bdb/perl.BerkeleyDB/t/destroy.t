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

print "1..13\n";


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
    # let object destroction kill everything

    my $lex = new LexFile $Dfile ;
    my %hash ;
    my $value ;

    rmtree $home if -e $home ;
    ok 1, mkdir($home, 0777) ;
    ok 2, my $env = new BerkeleyDB::Env -Home => $home,
				     -Flags => DB_CREATE|DB_INIT_TXN|
					  	DB_INIT_MPOOL|DB_INIT_LOCK ;
    ok 3, my $txn = $env->txn_begin() ;
    ok 4, my $db1 = tie %hash, 'BerkeleyDB::Hash', -Filename => $Dfile,
                                      	       	-Flags     => DB_CREATE ,
					       	-Env 	   => $env,
					    	-Txn	   => $txn  ;

    
    # create some data
    my %data =  (
		"red"	=> "boat",
		"green"	=> "house",
		"blue"	=> "sea",
		) ;

    my $ret = 0 ;
    while (my ($k, $v) = each %data) {
        $ret += $db1->db_put($k, $v) ;
    }
    ok 5, $ret == 0 ;

    # should be able to see all the records

    ok 6, my $cursor = $db1->db_cursor() ;
    my ($k, $v) = ("", "") ;
    my $count = 0 ;
    # sequence forwards
    while ($cursor->c_get($k, $v, DB_NEXT) == 0) {
        ++ $count ;
    }
    ok 7, $count == 3 ;
    undef $cursor ;

    # now abort the transaction
    ok 8, $txn->txn_abort() == 0 ;

    # there shouldn't be any records in the database
    $count = 0 ;
    # sequence forwards
    ok 9, $cursor = $db1->db_cursor() ;
    while ($cursor->c_get($k, $v, DB_NEXT) == 0) {
        ++ $count ;
    }
    ok 10, $count == 0 ;

    #undef $txn ;
    #undef $cursor ;
    #undef $db1 ;
    #undef $env ;
    #untie %hash ;

}
{
    my $lex = new LexFile $Dfile ;
    my %hash ;
    my $cursor ;
    my ($k, $v) = ("", "") ;
    ok 11, my $db1 = tie %hash, 'BerkeleyDB::Hash', 
		-Filename	=> $Dfile,
               	-Flags		=> DB_CREATE ;
    my $count = 0 ;
    # sequence forwards
    ok 12, $cursor = $db1->db_cursor() ;
    while ($cursor->c_get($k, $v, DB_NEXT) == 0) {
        ++ $count ;
    }
    ok 13, $count == 0 ;
}

rmtree $home ;

