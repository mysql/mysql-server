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
use File::Path qw(rmtree);

if ($BerkeleyDB::db_ver < 2.005002)
{
    print "1..0 # Skip: join needs Berkeley DB 2.5.2 or later\n" ;
    exit 0 ;
}


print "1..37\n";

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

my $Dfile1 = "dbhash1.tmp";
my $Dfile2 = "dbhash2.tmp";
my $Dfile3 = "dbhash3.tmp";
unlink $Dfile1, $Dfile2, $Dfile3 ;

umask(0) ;

sub addData
{
    my $db = shift ;
    my @data = @_ ;
    die "addData odd data\n" unless @data /2 != 0 ;
    my ($k, $v) ;
    my $ret = 0 ;
    while (@data) {
        $k = shift @data ;
        $v = shift @data ;
        $ret += $db->db_put($k, $v) ;
    }

    return ($ret == 0) ;
}

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
    ok 2, $@ =~ /Usage: \$db->BerkeleyDB::Common::db_join\Q([cursors], flags=0)/;

    # empty list
    eval '$cursor = $db1->db_join([]) ;' ;
    ok 3, $@ =~ /db_join: No cursors in parameter list/;

    # cursor list, isn't a []
    eval '$cursor = $db1->db_join({}) ;' ;
    ok 4, $@ =~ /cursors is not an array reference at/ ;

    eval '$cursor = $db1->db_join(\1) ;' ;
    ok 5, $@ =~ /cursors is not an array reference at/ ;

}

{
    # test a 2-way & 3-way join

    my $lex = new LexFile $Dfile1, $Dfile2, $Dfile3 ;
    my %hash1 ;
    my %hash2 ;
    my %hash3 ;
    my $value ;
    my $status ;

    my $home = "./fred" ;
    rmtree $home if -e $home ;
    ok 6, mkdir($home, 0777) ;
    ok 7, my $env = new BerkeleyDB::Env -Home => $home,
				     -Flags => DB_CREATE|DB_INIT_TXN
					  	|DB_INIT_MPOOL;
					  	#|DB_INIT_MPOOL| DB_INIT_LOCK;
    ok 8, my $txn = $env->txn_begin() ;
    ok 9, my $db1 = tie %hash1, 'BerkeleyDB::Hash', 
				-Filename => $Dfile1,
                               	-Flags     => DB_CREATE,
                                -DupCompare   => sub { $_[0] cmp $_[1] },
                                -Property  => DB_DUP|DB_DUPSORT,
			       	-Env 	   => $env,
			    	-Txn	   => $txn  ;
				;

    ok 10, my $db2 = tie %hash2, 'BerkeleyDB::Hash', 
				-Filename => $Dfile2,
                               	-Flags     => DB_CREATE,
                                -DupCompare   => sub { $_[0] cmp $_[1] },
                                -Property  => DB_DUP|DB_DUPSORT,
			       	-Env 	   => $env,
			    	-Txn	   => $txn  ;

    ok 11, my $db3 = tie %hash3, 'BerkeleyDB::Btree', 
				-Filename => $Dfile3,
                               	-Flags     => DB_CREATE,
                                -DupCompare   => sub { $_[0] cmp $_[1] },
                                -Property  => DB_DUP|DB_DUPSORT,
			       	-Env 	   => $env,
			    	-Txn	   => $txn  ;

    
    ok 12, addData($db1, qw( 	apple		Convenience
    				peach		Shopway
				pear		Farmer
				raspberry	Shopway
				strawberry	Shopway
				gooseberry	Farmer
				blueberry	Farmer
    			));

    ok 13, addData($db2, qw( 	red	apple
    				red	raspberry
    				red	strawberry
				yellow	peach
				yellow	pear
				green	gooseberry
				blue	blueberry)) ;

    ok 14, addData($db3, qw( 	expensive	apple
    				reasonable	raspberry
    				expensive	strawberry
				reasonable	peach
				reasonable	pear
				expensive	gooseberry
				reasonable	blueberry)) ;

    ok 15, my $cursor2 = $db2->db_cursor() ;
    my $k = "red" ;
    my $v = "" ;
    ok 16, $cursor2->c_get($k, $v, DB_SET) == 0 ;

    # Two way Join
    ok 17, my $cursor1 = $db1->db_join([$cursor2]) ;

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
    ok 18, keys %expected == 0 ;
    ok 19, $cursor1->status() == DB_NOTFOUND ;

    # Three way Join
    ok 20, $cursor2 = $db2->db_cursor() ;
    $k = "red" ;
    $v = "" ;
    ok 21, $cursor2->c_get($k, $v, DB_SET) == 0 ;

    ok 22, my $cursor3 = $db3->db_cursor() ;
    $k = "expensive" ;
    $v = "" ;
    ok 23, $cursor3->c_get($k, $v, DB_SET) == 0 ;
    ok 24, $cursor1 = $db1->db_join([$cursor2, $cursor3]) ;

    %expected = qw( apple Convenience
			strawberry Shopway
		) ;

    # sequence forwards
    while ($cursor1->c_get($k, $v) == 0) {
	delete $expected{$k} 
	    if defined $expected{$k} && $expected{$k} eq $v ;
	#print "[$k] [$v]\n" ;
    }
    ok 25, keys %expected == 0 ;
    ok 26, $cursor1->status() == DB_NOTFOUND ;

    # test DB_JOIN_ITEM
    # #################
    ok 27, $cursor2 = $db2->db_cursor() ;
    $k = "red" ;
    $v = "" ;
    ok 28, $cursor2->c_get($k, $v, DB_SET) == 0 ;
 
    ok 29, $cursor3 = $db3->db_cursor() ;
    $k = "expensive" ;
    $v = "" ;
    ok 30, $cursor3->c_get($k, $v, DB_SET) == 0 ;
    ok 31, $cursor1 = $db1->db_join([$cursor2, $cursor3]) ;
 
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
    ok 32, keys %expected == 0 ;
    ok 33, $cursor1->status() == DB_NOTFOUND ;

    ok 34, $cursor1->c_close() == 0 ;
    ok 35, $cursor2->c_close() == 0 ;
    ok 36, $cursor3->c_close() == 0 ;

    ok 37, ($status = $txn->txn_commit) == 0;

    undef $txn ;
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
    rmtree $home ;
}

