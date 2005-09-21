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

print "1..58\n";

my $Dfile = "dbhash.tmp";

umask(0);

{
    # error cases

    my $lex = new LexFile $Dfile ;
    my %hash ;
    my $value ;

    my $home = "./fred" ;
    ok 1, my $lexD = new LexDir($home);
    ok 2, my $env = new BerkeleyDB::Env -Home => $home, @StdErrFile,
				     -Flags => DB_CREATE| DB_INIT_MPOOL;
    eval { $env->txn_begin() ; } ;
    ok 3, $@ =~ /^BerkeleyDB Aborting: Transaction Manager not enabled at/ ;

    eval { my $txn_mgr = $env->TxnMgr() ; } ;
    ok 4, $@ =~ /^BerkeleyDB Aborting: Transaction Manager not enabled at/ ;
    undef $env ;

}

{
    # transaction - abort works

    my $lex = new LexFile $Dfile ;
    my %hash ;
    my $value ;

    my $home = "./fred" ;
    ok 5, my $lexD = new LexDir($home);
    ok 6, my $env = new BerkeleyDB::Env -Home => $home, @StdErrFile,
				     -Flags => DB_CREATE|DB_INIT_TXN|
					  	DB_INIT_MPOOL|DB_INIT_LOCK ;
    ok 7, my $txn = $env->txn_begin() ;
    ok 8, my $db1 = tie %hash, 'BerkeleyDB::Hash', -Filename => $Dfile,
                                      	       	-Flags     => DB_CREATE ,
					       	-Env 	   => $env,
					    	-Txn	   => $txn  ;

    
    ok 9, $txn->txn_commit() == 0 ;
    ok 10, $txn = $env->txn_begin() ;
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
    ok 11, $ret == 0 ;

    # should be able to see all the records

    ok 12, my $cursor = $db1->db_cursor() ;
    my ($k, $v) = ("", "") ;
    my $count = 0 ;
    # sequence forwards
    while ($cursor->c_get($k, $v, DB_NEXT) == 0) {
        ++ $count ;
    }
    ok 13, $count == 3 ;
    undef $cursor ;

    # now abort the transaction
    ok 14, $txn->txn_abort() == 0 ;

    # there shouldn't be any records in the database
    $count = 0 ;
    # sequence forwards
    ok 15, $cursor = $db1->db_cursor() ;
    while ($cursor->c_get($k, $v, DB_NEXT) == 0) {
        ++ $count ;
    }
    ok 16, $count == 0 ;

    my $stat = $env->txn_stat() ;
    ok 17, $stat->{'st_naborts'} == 1 ;

    undef $txn ;
    undef $cursor ;
    undef $db1 ;
    undef $env ;
    untie %hash ;
}

{
    # transaction - abort works via txnmgr

    my $lex = new LexFile $Dfile ;
    my %hash ;
    my $value ;

    my $home = "./fred" ;
    ok 18, my $lexD = new LexDir($home);
    ok 19, my $env = new BerkeleyDB::Env -Home => $home, @StdErrFile,
				     -Flags => DB_CREATE|DB_INIT_TXN|
					  	DB_INIT_MPOOL|DB_INIT_LOCK ;
    ok 20, my $txn_mgr = $env->TxnMgr() ;
    ok 21, my $txn = $txn_mgr->txn_begin() ;
    ok 22, my $db1 = tie %hash, 'BerkeleyDB::Hash', -Filename => $Dfile,
                                      	       	-Flags     => DB_CREATE ,
					       	-Env 	   => $env,
					    	-Txn	   => $txn  ;

    ok 23, $txn->txn_commit() == 0 ;
    ok 24, $txn = $env->txn_begin() ;
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
    ok 25, $ret == 0 ;

    # should be able to see all the records

    ok 26, my $cursor = $db1->db_cursor() ;
    my ($k, $v) = ("", "") ;
    my $count = 0 ;
    # sequence forwards
    while ($cursor->c_get($k, $v, DB_NEXT) == 0) {
        ++ $count ;
    }
    ok 27, $count == 3 ;
    undef $cursor ;

    # now abort the transaction
    ok 28, $txn->txn_abort() == 0 ;

    # there shouldn't be any records in the database
    $count = 0 ;
    # sequence forwards
    ok 29, $cursor = $db1->db_cursor() ;
    while ($cursor->c_get($k, $v, DB_NEXT) == 0) {
        ++ $count ;
    }
    ok 30, $count == 0 ;

    my $stat = $txn_mgr->txn_stat() ;
    ok 31, $stat->{'st_naborts'} == 1 ;

    undef $txn ;
    undef $cursor ;
    undef $db1 ;
    undef $txn_mgr ;
    undef $env ;
    untie %hash ;
}

{
    # transaction - commit works

    my $lex = new LexFile $Dfile ;
    my %hash ;
    my $value ;

    my $home = "./fred" ;
    ok 32, my $lexD = new LexDir($home);
    ok 33, my $env = new BerkeleyDB::Env -Home => $home, @StdErrFile,
				     -Flags => DB_CREATE|DB_INIT_TXN|
					  	DB_INIT_MPOOL|DB_INIT_LOCK ;
    ok 34, my $txn = $env->txn_begin() ;
    ok 35, my $db1 = tie %hash, 'BerkeleyDB::Hash', -Filename => $Dfile,
                                      	       	-Flags     => DB_CREATE ,
					       	-Env 	   => $env,
					    	-Txn	   => $txn  ;

    
    ok 36, $txn->txn_commit() == 0 ;
    ok 37, $txn = $env->txn_begin() ;
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
    ok 38, $ret == 0 ;

    # should be able to see all the records

    ok 39, my $cursor = $db1->db_cursor() ;
    my ($k, $v) = ("", "") ;
    my $count = 0 ;
    # sequence forwards
    while ($cursor->c_get($k, $v, DB_NEXT) == 0) {
        ++ $count ;
    }
    ok 40, $count == 3 ;
    undef $cursor ;

    # now commit the transaction
    ok 41, $txn->txn_commit() == 0 ;

    $count = 0 ;
    # sequence forwards
    ok 42, $cursor = $db1->db_cursor() ;
    while ($cursor->c_get($k, $v, DB_NEXT) == 0) {
        ++ $count ;
    }
    ok 43, $count == 3 ;

    my $stat = $env->txn_stat() ;
    ok 44, $stat->{'st_naborts'} == 0 ;

    undef $txn ;
    undef $cursor ;
    undef $db1 ;
    undef $env ;
    untie %hash ;
}

{
    # transaction - commit works via txnmgr

    my $lex = new LexFile $Dfile ;
    my %hash ;
    my $value ;

    my $home = "./fred" ;
    ok 45, my $lexD = new LexDir($home);
    ok 46, my $env = new BerkeleyDB::Env -Home => $home, @StdErrFile,
				     -Flags => DB_CREATE|DB_INIT_TXN|
					  	DB_INIT_MPOOL|DB_INIT_LOCK ;
    ok 47, my $txn_mgr = $env->TxnMgr() ;
    ok 48, my $txn = $txn_mgr->txn_begin() ;
    ok 49, my $db1 = tie %hash, 'BerkeleyDB::Hash', -Filename => $Dfile,
                                      	       	-Flags     => DB_CREATE ,
					       	-Env 	   => $env,
					    	-Txn	   => $txn  ;

    ok 50, $txn->txn_commit() == 0 ;
    ok 51, $txn = $env->txn_begin() ;
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
    ok 52, $ret == 0 ;

    # should be able to see all the records

    ok 53, my $cursor = $db1->db_cursor() ;
    my ($k, $v) = ("", "") ;
    my $count = 0 ;
    # sequence forwards
    while ($cursor->c_get($k, $v, DB_NEXT) == 0) {
        ++ $count ;
    }
    ok 54, $count == 3 ;
    undef $cursor ;

    # now commit the transaction
    ok 55, $txn->txn_commit() == 0 ;

    $count = 0 ;
    # sequence forwards
    ok 56, $cursor = $db1->db_cursor() ;
    while ($cursor->c_get($k, $v, DB_NEXT) == 0) {
        ++ $count ;
    }
    ok 57, $count == 3 ;

    my $stat = $txn_mgr->txn_stat() ;
    ok 58, $stat->{'st_naborts'} == 0 ;

    undef $txn ;
    undef $cursor ;
    undef $db1 ;
    undef $txn_mgr ;
    undef $env ;
    untie %hash ;
}

