#!./perl -w

# ID: %I%, %G%   

use strict ;

BEGIN {
    unless(grep /blib/, @INC) {
        chdir 't' if -d 't';
        @INC = '../lib' if -d '../lib';
    }
}

use BerkeleyDB; 
use t::util ;

if ($BerkeleyDB::db_ver < 2.005002)
{
    print "1..0 # Skip: join needs Berkeley DB 2.5.2 or later\n" ;
    exit 0 ;
}


print "1..42\n";

my $Dfile1 = "dbhash1.tmp";
my $Dfile2 = "dbhash2.tmp";
my $Dfile3 = "dbhash3.tmp";
unlink $Dfile1, $Dfile2, $Dfile3 ;

umask(0) ;

{
    # error cases
    my $lex = new LexFile $Dfile1, $Dfile2, $Dfile3 ;
    my %hash1 ;
    my $value ;
    my $status ;
    my $cursor ;

    ok 1, my $db1 = tie %hash1, 'BerkeleyDB::Hash', 
				-Filename => $Dfile1,
                               	-Flags     => DB_CREATE,
                                -DupCompare   => sub { $_[0] lt $_[1] },
                                -Property  => DB_DUP|DB_DUPSORT ;

    # no cursors supplied
    eval '$cursor = $db1->db_join() ;' ;
    ok 2, $@ =~ /Usage: \$db->BerkeleyDB::db_join\Q([cursors], flags=0)/;

    # empty list
    eval '$cursor = $db1->db_join([]) ;' ;
    ok 3, $@ =~ /db_join: No cursors in parameter list/;

    # cursor list, isn not a []
    eval '$cursor = $db1->db_join({}) ;' ;
    ok 4, $@ =~ /db_join: first parameter is not an array reference/;

    eval '$cursor = $db1->db_join(\1) ;' ;
    ok 5, $@ =~ /db_join: first parameter is not an array reference/;

    my ($a, $b) = ("a", "b");
    $a = bless [], "fred";
    $b = bless [], "fred";
    eval '$cursor = $db1->db_join($a, $b) ;' ;
    ok 6, $@ =~ /db_join: first parameter is not an array reference/;

}

{
    # test a 2-way & 3-way join

    my $lex = new LexFile $Dfile1, $Dfile2, $Dfile3 ;
    my %hash1 ;
    my %hash2 ;
    my %hash3 ;
    my $value ;
    my $status ;

    my $home = "./fred7" ;
    rmtree $home;
    ok 7, ! -d $home;
    ok 8, my $lexD = new LexDir($home);
    ok 9, my $env = new BerkeleyDB::Env -Home => $home, @StdErrFile,
				     -Flags => DB_CREATE|DB_INIT_TXN
					  	|DB_INIT_MPOOL;
					  	#|DB_INIT_MPOOL| DB_INIT_LOCK;
    ok 10, my $txn = $env->txn_begin() ;
    ok 11, my $db1 = tie %hash1, 'BerkeleyDB::Hash', 
				-Filename => $Dfile1,
                               	-Flags     => DB_CREATE,
                                -DupCompare   => sub { $_[0] cmp $_[1] },
                                -Property  => DB_DUP|DB_DUPSORT,
			       	-Env 	   => $env,
			    	-Txn	   => $txn  ;
				;

    ok 12, my $db2 = tie %hash2, 'BerkeleyDB::Hash', 
				-Filename => $Dfile2,
                               	-Flags     => DB_CREATE,
                                -DupCompare   => sub { $_[0] cmp $_[1] },
                                -Property  => DB_DUP|DB_DUPSORT,
			       	-Env 	   => $env,
			    	-Txn	   => $txn  ;

    ok 13, my $db3 = tie %hash3, 'BerkeleyDB::Btree', 
				-Filename => $Dfile3,
                               	-Flags     => DB_CREATE,
                                -DupCompare   => sub { $_[0] cmp $_[1] },
                                -Property  => DB_DUP|DB_DUPSORT,
			       	-Env 	   => $env,
			    	-Txn	   => $txn  ;

    
    ok 14, addData($db1, qw( 	apple		Convenience
    				peach		Shopway
				pear		Farmer
				raspberry	Shopway
				strawberry	Shopway
				gooseberry	Farmer
				blueberry	Farmer
    			));

    ok 15, addData($db2, qw( 	red	apple
    				red	raspberry
    				red	strawberry
				yellow	peach
				yellow	pear
				green	gooseberry
				blue	blueberry)) ;

    ok 16, addData($db3, qw( 	expensive	apple
    				reasonable	raspberry
    				expensive	strawberry
				reasonable	peach
				reasonable	pear
				expensive	gooseberry
				reasonable	blueberry)) ;

    ok 17, my $cursor2 = $db2->db_cursor() ;
    my $k = "red" ;
    my $v = "" ;
    ok 18, $cursor2->c_get($k, $v, DB_SET) == 0 ;

    # Two way Join
    ok 19, my $cursor1 = $db1->db_join([$cursor2]) ;

    my %expected = qw( apple Convenience
			raspberry Shopway
			strawberry Shopway
		) ;

    # sequence forwards
    while ($cursor1->c_get($k, $v) == 0) {
	delete $expected{$k} 
	    if defined $expected{$k} && $expected{$k} eq $v ;
	#print "[$k] [$v]\n" ;
    }
    ok 20, keys %expected == 0 ;
    ok 21, $cursor1->status() == DB_NOTFOUND ;

    # Three way Join
    ok 22, $cursor2 = $db2->db_cursor() ;
    $k = "red" ;
    $v = "" ;
    ok 23, $cursor2->c_get($k, $v, DB_SET) == 0 ;

    ok 24, my $cursor3 = $db3->db_cursor() ;
    $k = "expensive" ;
    $v = "" ;
    ok 25, $cursor3->c_get($k, $v, DB_SET) == 0 ;
    ok 26, $cursor1 = $db1->db_join([$cursor2, $cursor3]) ;

    %expected = qw( apple Convenience
			strawberry Shopway
		) ;

    # sequence forwards
    while ($cursor1->c_get($k, $v) == 0) {
	delete $expected{$k} 
	    if defined $expected{$k} && $expected{$k} eq $v ;
	#print "[$k] [$v]\n" ;
    }
    ok 27, keys %expected == 0 ;
    ok 28, $cursor1->status() == DB_NOTFOUND ;

    # test DB_JOIN_ITEM
    # #################
    ok 29, $cursor2 = $db2->db_cursor() ;
    $k = "red" ;
    $v = "" ;
    ok 30, $cursor2->c_get($k, $v, DB_SET) == 0 ;
 
    ok 31, $cursor3 = $db3->db_cursor() ;
    $k = "expensive" ;
    $v = "" ;
    ok 32, $cursor3->c_get($k, $v, DB_SET) == 0 ;
    ok 33, $cursor1 = $db1->db_join([$cursor2, $cursor3]) ;
 
    %expected = qw( apple 1
                        strawberry 1
                ) ;
 
    # sequence forwards
    $k = "" ;
    $v = "" ;
    while ($cursor1->c_get($k, $v, DB_JOIN_ITEM) == 0) {
        delete $expected{$k}
            if defined $expected{$k} ;
        #print "[$k]\n" ;
    }
    ok 34, keys %expected == 0 ;
    ok 35, $cursor1->status() == DB_NOTFOUND ;

    ok 36, $cursor1->c_close() == 0 ;
    ok 37, $cursor2->c_close() == 0 ;
    ok 38, $cursor3->c_close() == 0 ;

    ok 39, ($status = $txn->txn_commit()) == 0;

    undef $txn ;

    ok 40, my $cursor1a = $db1->db_cursor() ;
    eval { $cursor1 = $db1->db_join([$cursor1a]) };
    ok 41, $@ =~ /BerkeleyDB Aborting: attempted to do a self-join at/;
    eval { $cursor1 = $db1->db_join([$cursor1]) } ;
    ok 42, $@ =~ /BerkeleyDB Aborting: attempted to do a self-join at/;

    undef $cursor1a;
    #undef $cursor1;
    #undef $cursor2;
    #undef $cursor3;
    undef $db1 ;
    undef $db2 ;
    undef $db3 ;
    undef $env ;
    untie %hash1 ;
    untie %hash2 ;
    untie %hash3 ;
}

print "# at the end\n";
