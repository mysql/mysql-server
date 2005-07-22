#!./perl -w

use strict ;

BEGIN {
    unless(grep /blib/, @INC) {
        chdir 't' if -d 't';
        @INC = '../lib' if -d '../lib';
    }
}

use BerkeleyDB; 
use t::util ;

print "1..15\n";

my $Dfile = "dbhash.tmp";
my $home = "./fred" ;

umask(0);

{
    # let object destruction kill everything

    my $lex = new LexFile $Dfile ;
    my %hash ;
    my $value ;

    ok 1, my $lexD = new LexDir($home) ;
    ok 2, my $env = new BerkeleyDB::Env -Home => $home,  @StdErrFile,
				     -Flags => DB_CREATE|DB_INIT_TXN|
					  	DB_INIT_MPOOL|DB_INIT_LOCK ;
    ok 3, my $txn = $env->txn_begin() ;
    ok 4, my $db1 = tie %hash, 'BerkeleyDB::Hash', -Filename => $Dfile,
                                      	       	-Flags     => DB_CREATE ,
					       	-Env 	   => $env,
					    	-Txn	   => $txn  ;

    ok 5, $txn->txn_commit() == 0 ;
    ok 6, $txn = $env->txn_begin() ;
    $db1->Txn($txn);
    
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
    ok 7, $ret == 0 ;

    # should be able to see all the records

    ok 8, my $cursor = $db1->db_cursor() ;
    my ($k, $v) = ("", "") ;
    my $count = 0 ;
    # sequence forwards
    while ($cursor->c_get($k, $v, DB_NEXT) == 0) {
        ++ $count ;
    }
    ok 9, $count == 3 ;
    undef $cursor ;

    # now abort the transaction
    ok 10, $txn->txn_abort() == 0 ;

    # there shouldn't be any records in the database
    $count = 0 ;
    # sequence forwards
    ok 11, $cursor = $db1->db_cursor() ;
    while ($cursor->c_get($k, $v, DB_NEXT) == 0) {
        ++ $count ;
    }
    ok 12, $count == 0 ;

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
    ok 13, my $db1 = tie %hash, 'BerkeleyDB::Hash', 
		-Filename	=> $Dfile,
               	-Flags		=> DB_CREATE ;
    my $count = 0 ;
    # sequence forwards
    ok 14, $cursor = $db1->db_cursor() ;
    while ($cursor->c_get($k, $v, DB_NEXT) == 0) {
        ++ $count ;
    }
    ok 15, $count == 0 ;
}


