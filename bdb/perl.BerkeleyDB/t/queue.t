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

BEGIN 
{
    if ($BerkeleyDB::db_version < 3) {
	print "1..0 # Skipping test, Queue needs Berkeley DB 3.x or better\n" ;
	exit 0 ;
    }
}    

print "1..197\n";

my %DB_errors = (
	'DB_INCOMPLETE'	=> "DB_INCOMPLETE: Sync was unable to complete",
	'DB_KEYEMPTY'	=> "DB_KEYEMPTY: Non-existent key/data pair",
	'DB_KEYEXIST'	=> "DB_KEYEXIST: Key/data pair already exists",
	'DB_LOCK_DEADLOCK' => "DB_LOCK_DEADLOCK: Locker killed to resolve a deadlock",
	'DB_LOCK_NOTGRANTED' => "DB_LOCK_NOTGRANTED: Lock not granted",
	'DB_NOTFOUND'	=> "DB_NOTFOUND: No matching key/data pair found",
	'DB_OLD_VERSION'=> "DB_OLDVERSION: Database requires a version upgrade",
	'DB_RUNRECOVERY'=> "DB_RUNRECOVERY: Fatal error, run database recovery",
	) ;

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

sub touch
{
    my $file = shift ;
    open(CAT,">$file") || die "Cannot open $file:$!";
    close(CAT);
}

sub joiner
{
    my $db = shift ;
    my $sep = shift ;
    my ($k, $v) = (0, "") ;
    my @data = () ;

    my $cursor = $db->db_cursor()  or return () ;
    for ( my $status = $cursor->c_get($k, $v, DB_FIRST) ;
          $status == 0 ;
          $status = $cursor->c_get($k, $v, DB_NEXT)) {
	push @data, $v ;
    }

    (scalar(@data), join($sep, @data)) ;
}

sub countRecords
{
   my $db = shift ;
   my ($k, $v) = (0,0) ;
   my ($count) = 0 ;
   my ($cursor) = $db->db_cursor() ;
   #for ($status = $cursor->c_get($k, $v, DB_FIRST) ;
#	$status == 0 ;
#	$status = $cursor->c_get($k, $v, DB_NEXT) )
   while ($cursor->c_get($k, $v, DB_NEXT) == 0)
     { ++ $count }

   return $count ;
}

sub fillout
{
    my $var = shift ;
    my $length = shift ;
    my $pad = shift || " " ;
    my $template = $pad x $length ;
    substr($template, 0, length($var)) = $var ;
    return $template ;
}
 
my $Dfile = "dbhash.tmp";
my $Dfile2 = "dbhash2.tmp";
my $Dfile3 = "dbhash3.tmp";
unlink $Dfile;

umask(0) ;


# Check for invalid parameters
{
    # Check for invalid parameters
    my $db ;
    eval ' $db = new BerkeleyDB::Queue  -Stupid => 3 ; ' ;
    ok 1, $@ =~ /unknown key value\(s\) Stupid/  ;

    eval ' $db = new BerkeleyDB::Queue -Bad => 2, -Mode => 0345, -Stupid => 3; ' ;
    ok 2, $@ =~ /unknown key value\(s\) /  ;

    eval ' $db = new BerkeleyDB::Queue -Env => 2 ' ;
    ok 3, $@ =~ /^Env not of type BerkeleyDB::Env/ ;

    eval ' $db = new BerkeleyDB::Queue -Txn => "x" ' ;
    ok 4, $@ =~ /^Txn not of type BerkeleyDB::Txn/ ;

    my $obj = bless [], "main" ;
    eval ' $db = new BerkeleyDB::Queue -Env => $obj ' ;
    ok 5, $@ =~ /^Env not of type BerkeleyDB::Env/ ;
}

# Now check the interface to Queue

{
    my $lex = new LexFile $Dfile ;
    my $rec_len = 10 ;
    my $pad = "x" ;

    ok 6, my $db = new BerkeleyDB::Queue -Filename => $Dfile, 
				    -Flags    => DB_CREATE,
				    -Len      => $rec_len,
				    -Pad      => $pad;

    # Add a k/v pair
    my $value ;
    my $status ;
    ok 7, $db->db_put(1, "some value") == 0  ;
    ok 8, $db->status() == 0 ;
    ok 9, $db->db_get(1, $value) == 0 ;
    ok 10, $value eq fillout("some value", $rec_len, $pad) ;
    ok 11, $db->db_put(2, "value") == 0  ;
    ok 12, $db->db_get(2, $value) == 0 ;
    ok 13, $value eq fillout("value", $rec_len, $pad) ;
    ok 14, $db->db_del(1) == 0 ;
    ok 15, ($status = $db->db_get(1, $value)) == DB_KEYEMPTY ;
    ok 16, $db->status() == DB_KEYEMPTY ;
    ok 17, $db->status() eq $DB_errors{'DB_KEYEMPTY'} ;

    ok 18, ($status = $db->db_get(7, $value)) == DB_NOTFOUND ;
    ok 19, $db->status() == DB_NOTFOUND ;
    ok 20, $db->status() eq $DB_errors{'DB_NOTFOUND'} ;

    ok 21, $db->db_sync() == 0 ;

    # Check NOOVERWRITE will make put fail when attempting to overwrite
    # an existing record.

    ok 22, $db->db_put( 2, 'x', DB_NOOVERWRITE) == DB_KEYEXIST ;
    ok 23, $db->status() eq $DB_errors{'DB_KEYEXIST'} ;
    ok 24, $db->status() == DB_KEYEXIST ;


    # check that the value of the key  has not been changed by the
    # previous test
    ok 25, $db->db_get(2, $value) == 0 ;
    ok 26, $value eq fillout("value", $rec_len, $pad) ;


}


{
    # Check simple env works with a array.
    # and pad defaults to space
    my $lex = new LexFile $Dfile ;

    my $home = "./fred" ;
    my $rec_len = 11 ;
    ok 27, -d $home ? chmod 0777, $home : mkdir($home, 0777) ;

    ok 28, my $env = new BerkeleyDB::Env -Flags => DB_CREATE|DB_INIT_MPOOL,
    					 -Home => $home ;

    ok 29, my $db = new BerkeleyDB::Queue -Filename => $Dfile, 
				    -Env      => $env,
				    -Flags    => DB_CREATE,
				    -Len      => $rec_len;

    # Add a k/v pair
    my $value ;
    ok 30, $db->db_put(1, "some value") == 0 ;
    ok 31, $db->db_get(1, $value) == 0 ;
    ok 32, $value eq fillout("some value", $rec_len)  ;
    undef $db ;
    undef $env ;
    rmtree $home ;
}

 
{
    # cursors

    my $lex = new LexFile $Dfile ;
    my @array ;
    my ($k, $v) ;
    my $rec_len = 5 ;
    ok 33, my $db = new BerkeleyDB::Queue -Filename  => $Dfile, 
				    	  -ArrayBase => 0,
				    	  -Flags     => DB_CREATE ,
				    	  -Len       => $rec_len;

    # create some data
    my @data =  (
		"red"	,
		"green"	,
		"blue"	,
		) ;

    my $i ;
    my %data ;
    my $ret = 0 ;
    for ($i = 0 ; $i < @data ; ++$i) {
        $ret += $db->db_put($i, $data[$i]) ;
	$data{$i} = $data[$i] ;
    }
    ok 34, $ret == 0 ;

    # create the cursor
    ok 35, my $cursor = $db->db_cursor() ;

    $k = 0 ; $v = "" ;
    my %copy = %data;
    my $extras = 0 ;
    # sequence forwards
    while ($cursor->c_get($k, $v, DB_NEXT) == 0) 
    {
        if ( fillout($copy{$k}, $rec_len) eq $v ) 
            { delete $copy{$k} }
	else
	    { ++ $extras }
    }

    ok 36, $cursor->status() == DB_NOTFOUND ;
    ok 37, $cursor->status() eq $DB_errors{'DB_NOTFOUND'} ;
    ok 38, keys %copy == 0 ;
    ok 39, $extras == 0 ;

    # sequence backwards
    %copy = %data ;
    $extras = 0 ;
    my $status ;
    for ( $status = $cursor->c_get($k, $v, DB_LAST) ;
	  $status == 0 ;
    	  $status = $cursor->c_get($k, $v, DB_PREV)) {
        if ( fillout($copy{$k}, $rec_len) eq $v ) 
            { delete $copy{$k} }
	else
	    { ++ $extras }
    }
    ok 40, $status == DB_NOTFOUND ;
    ok 41, $status eq $DB_errors{'DB_NOTFOUND'} ;
    ok 42, $cursor->status() == $status ;
    ok 43, $cursor->status() eq $status ;
    ok 44, keys %copy == 0 ;
    ok 45, $extras == 0 ;
}
 
{
    # Tied Array interface

    # full tied array support started in Perl 5.004_57
    # just double check.
    my $FA = 0 ;
    {
        sub try::TIEARRAY { bless [], "try" }
        sub try::FETCHSIZE { $FA = 1 }
        my @a ; 
        tie @a, 'try' ;
        my $a = @a ;
    }

    my $lex = new LexFile $Dfile ;
    my @array ;
    my $db ;
    my $rec_len = 10 ;
    ok 46, $db = tie @array, 'BerkeleyDB::Queue', -Filename  => $Dfile,
				    	    -ArrayBase => 0,
                                            -Flags     => DB_CREATE ,
				    	    -Len       => $rec_len;

    ok 47, my $cursor = (tied @array)->db_cursor() ;
    # check the database is empty
    my $count = 0 ;
    my ($k, $v) = (0,"") ;
    while ($cursor->c_get($k, $v, DB_NEXT) == 0) {
	++ $count ;
    }
    ok 48, $cursor->status() == DB_NOTFOUND ;
    ok 49, $count == 0 ;

    ok 50, @array == 0 ;

    # Add a k/v pair
    my $value ;
    $array[1] = "some value";
    ok 51, (tied @array)->status() == 0 ;
    ok 52, $array[1] eq fillout("some value", $rec_len);
    ok 53, defined $array[1];
    ok 54, (tied @array)->status() == 0 ;
    ok 55, !defined $array[3];
    ok 56, (tied @array)->status() == DB_NOTFOUND ;

    ok 57, (tied @array)->db_del(1) == 0 ;
    ok 58, (tied @array)->status() == 0 ;
    ok 59, ! defined $array[1];
    ok 60, (tied @array)->status() == DB_KEYEMPTY ;

    $array[1] = 2 ;
    $array[10] = 20 ;
    $array[1000] = 2000 ;

    my ($keys, $values) = (0,0);
    $count = 0 ;
    for ( my $status = $cursor->c_get($k, $v, DB_FIRST) ;
	  $status == 0 ;
    	  $status = $cursor->c_get($k, $v, DB_NEXT)) {
        $keys += $k ;
	$values += $v ;
	++ $count ;
    }
    ok 61, $count == 3 ;
    ok 62, $keys == 1011 ;
    ok 63, $values == 2022 ;

    # unshift isn't allowed
#    eval {
#    	$FA ? unshift @array, "red", "green", "blue" 
#        : $db->unshift("red", "green", "blue" ) ;
#	} ;
#    ok 64, $@ =~ /^unshift is unsupported with Queue databases/ ;	
    $array[0] = "red" ;
    $array[1] = "green" ;
    $array[2] = "blue" ;
    $array[4] = 2 ;
    ok 64, $array[0] eq fillout("red", $rec_len) ;
    ok 65, $cursor->c_get($k, $v, DB_FIRST) == 0 ;
    ok 66, $k == 0 ;
    ok 67, $v eq fillout("red", $rec_len) ;
    ok 68, $array[1] eq fillout("green", $rec_len) ;
    ok 69, $cursor->c_get($k, $v, DB_NEXT) == 0 ;
    ok 70, $k == 1 ;
    ok 71, $v eq fillout("green", $rec_len) ;
    ok 72, $array[2] eq fillout("blue", $rec_len) ;
    ok 73, $cursor->c_get($k, $v, DB_NEXT) == 0 ;
    ok 74, $k == 2 ;
    ok 75, $v eq fillout("blue", $rec_len) ;
    ok 76, $array[4] == 2 ;
    ok 77, $cursor->c_get($k, $v, DB_NEXT) == 0 ;
    ok 78, $k == 4 ;
    ok 79, $v == 2 ;

    # shift
    ok 80, ($FA ? shift @array : $db->shift()) eq fillout("red", $rec_len) ;
    ok 81, ($FA ? shift @array : $db->shift()) eq fillout("green", $rec_len) ;
    ok 82, ($FA ? shift @array : $db->shift()) eq fillout("blue", $rec_len) ;
    ok 83, ($FA ? shift @array : $db->shift()) == 2 ;

    # push
    $FA ? push @array, "the", "end" 
        : $db->push("the", "end") ;
    ok 84, $cursor->c_get($k, $v, DB_LAST) == 0 ;
    ok 85, $k == 1002 ;
    ok 86, $v eq fillout("end", $rec_len) ;
    ok 87, $cursor->c_get($k, $v, DB_PREV) == 0 ;
    ok 88, $k == 1001 ;
    ok 89, $v eq fillout("the", $rec_len) ;
    ok 90, $cursor->c_get($k, $v, DB_PREV) == 0 ;
    ok 91, $k == 1000 ;
    ok 92, $v == 2000 ;

    # pop
    ok 93, ( $FA ? pop @array : $db->pop ) eq fillout("end", $rec_len) ;
    ok 94, ( $FA ? pop @array : $db->pop ) eq fillout("the", $rec_len) ;
    ok 95, ( $FA ? pop @array : $db->pop ) == 2000  ;

    # now clear the array 
    $FA ? @array = () 
        : $db->clear() ;
    ok 96, $cursor->c_get($k, $v, DB_FIRST) == DB_NOTFOUND ;

    undef $cursor ;
    undef $db ;
    untie @array ;
}

{
    # in-memory file

    my @array ;
    my $fd ;
    my $value ;
    my $rec_len = 15 ;
    ok 97, my $db = tie @array, 'BerkeleyDB::Queue',
				    	    -Len       => $rec_len;

    ok 98, $db->db_put(1, "some value") == 0  ;
    ok 99, $db->db_get(1, $value) == 0 ;
    ok 100, $value eq fillout("some value", $rec_len) ;

}
 
{
    # partial
    # check works via API

    my $lex = new LexFile $Dfile ;
    my $value ;
    my $rec_len = 8 ;
    ok 101, my $db = new BerkeleyDB::Queue  -Filename => $Dfile,
                                      	    -Flags    => DB_CREATE ,
				    	    -Len      => $rec_len,
				    	    -Pad      => " " ;

    # create some data
    my @data =  (
		"",
		"boat",
		"house",
		"sea",
		) ;

    my $ret = 0 ;
    my $i ;
    for ($i = 0 ; $i < @data ; ++$i) {
        my $r = $db->db_put($i, $data[$i]) ;
        $ret += $r ;
    }
    ok 102, $ret == 0 ;

    # do a partial get
    my ($pon, $off, $len) = $db->partial_set(0,2) ;
    ok 103, ! $pon && $off == 0 && $len == 0 ;
    ok 104, $db->db_get(1, $value) == 0 && $value eq "bo" ;
    ok 105, $db->db_get(2, $value) == 0 && $value eq "ho" ;
    ok 106, $db->db_get(3, $value) == 0 && $value eq "se" ;

    # do a partial get, off end of data
    ($pon, $off, $len) = $db->partial_set(3,2) ;
    ok 107, $pon ;
    ok 108, $off == 0 ;
    ok 109, $len == 2 ;
    ok 110, $db->db_get(1, $value) == 0 && $value eq fillout("t", 2) ;
    ok 111, $db->db_get(2, $value) == 0 && $value eq "se" ;
    ok 112, $db->db_get(3, $value) == 0 && $value eq "  " ;

    # switch of partial mode
    ($pon, $off, $len) = $db->partial_clear() ;
    ok 113, $pon ;
    ok 114, $off == 3 ;
    ok 115, $len == 2 ;
    ok 116, $db->db_get(1, $value) == 0 && $value eq fillout("boat", $rec_len) ;
    ok 117, $db->db_get(2, $value) == 0 && $value eq fillout("house", $rec_len) ;
    ok 118, $db->db_get(3, $value) == 0 && $value eq fillout("sea", $rec_len) ;

    # now partial put
    $db->partial_set(0,2) ;
    ok 119, $db->db_put(1, "") != 0 ;
    ok 120, $db->db_put(2, "AB") == 0 ;
    ok 121, $db->db_put(3, "XY") == 0 ;
    ok 122, $db->db_put(4, "KLM") != 0 ;
    ok 123, $db->db_put(4, "KL") == 0 ;

    ($pon, $off, $len) = $db->partial_clear() ;
    ok 124, $pon ;
    ok 125, $off == 0 ;
    ok 126, $len == 2 ;
    ok 127, $db->db_get(1, $value) == 0 && $value eq fillout("boat", $rec_len) ;
    ok 128, $db->db_get(2, $value) == 0 && $value eq fillout("ABuse", $rec_len) ;
    ok 129, $db->db_get(3, $value) == 0 && $value eq fillout("XYa", $rec_len) ;
    ok 130, $db->db_get(4, $value) == 0 && $value eq fillout("KL", $rec_len) ;

    # now partial put
    ($pon, $off, $len) = $db->partial_set(3,2) ;
    ok 131, ! $pon ;
    ok 132, $off == 0 ;
    ok 133, $len == 0 ;
    ok 134, $db->db_put(1, "PP") == 0 ;
    ok 135, $db->db_put(2, "Q") != 0 ;
    ok 136, $db->db_put(3, "XY") == 0 ;
    ok 137, $db->db_put(4, "TU") == 0 ;

    $db->partial_clear() ;
    ok 138, $db->db_get(1, $value) == 0 && $value eq fillout("boaPP", $rec_len) ;
    ok 139, $db->db_get(2, $value) == 0 && $value eq fillout("ABuse",$rec_len) ;
    ok 140, $db->db_get(3, $value) == 0 && $value eq fillout("XYaXY", $rec_len) ;
    ok 141, $db->db_get(4, $value) == 0 && $value eq fillout("KL TU", $rec_len) ;
}

{
    # partial
    # check works via tied array 

    my $lex = new LexFile $Dfile ;
    my @array ;
    my $value ;
    my $rec_len = 8 ;
    ok 142, my $db = tie @array, 'BerkeleyDB::Queue', -Filename => $Dfile,
                                      	        -Flags    => DB_CREATE ,
				    	        -Len       => $rec_len,
				    	        -Pad       => " " ;

    # create some data
    my @data =  (
		"",
		"boat",
		"house",
		"sea",
		) ;

    my $i ;
    my $status = 0 ;
    for ($i = 1 ; $i < @data ; ++$i) {
	$array[$i] = $data[$i] ;
	$status += $db->status() ;
    }

    ok 143, $status == 0 ;

    # do a partial get
    $db->partial_set(0,2) ;
    ok 144, $array[1] eq fillout("bo", 2) ;
    ok 145, $array[2] eq fillout("ho", 2) ;
    ok 146, $array[3]  eq fillout("se", 2) ;

    # do a partial get, off end of data
    $db->partial_set(3,2) ;
    ok 147, $array[1] eq fillout("t", 2) ;
    ok 148, $array[2] eq fillout("se", 2) ;
    ok 149, $array[3] eq fillout("", 2) ;

    # switch of partial mode
    $db->partial_clear() ;
    ok 150, $array[1] eq fillout("boat", $rec_len) ;
    ok 151, $array[2] eq fillout("house", $rec_len) ;
    ok 152, $array[3] eq fillout("sea", $rec_len) ;

    # now partial put
    $db->partial_set(0,2) ;
    $array[1] = "" ;
    ok 153, $db->status() != 0 ;
    $array[2] = "AB" ;
    ok 154, $db->status() == 0 ;
    $array[3] = "XY" ;
    ok 155, $db->status() == 0 ;
    $array[4] = "KL" ;
    ok 156, $db->status() == 0 ;

    $db->partial_clear() ;
    ok 157, $array[1] eq fillout("boat", $rec_len) ;
    ok 158, $array[2] eq fillout("ABuse", $rec_len) ;
    ok 159, $array[3] eq fillout("XYa", $rec_len) ;
    ok 160, $array[4] eq fillout("KL", $rec_len) ;

    # now partial put
    $db->partial_set(3,2) ;
    $array[1] = "PP" ;
    ok 161, $db->status() == 0 ;
    $array[2] = "Q" ;
    ok 162, $db->status() != 0 ;
    $array[3] = "XY" ;
    ok 163, $db->status() == 0 ;
    $array[4] = "TU" ;
    ok 164, $db->status() == 0 ;

    $db->partial_clear() ;
    ok 165, $array[1] eq fillout("boaPP", $rec_len) ;
    ok 166, $array[2] eq fillout("ABuse", $rec_len) ;
    ok 167, $array[3] eq fillout("XYaXY", $rec_len) ;
    ok 168, $array[4] eq fillout("KL TU", $rec_len) ;
}

{
    # transaction

    my $lex = new LexFile $Dfile ;
    my @array ;
    my $value ;

    my $home = "./fred" ;
    rmtree $home if -e $home ;
    ok 169, mkdir($home, 0777) ;
    my $rec_len = 9 ;
    ok 170, my $env = new BerkeleyDB::Env -Home => $home,
				     -Flags => DB_CREATE|DB_INIT_TXN|
					  	DB_INIT_MPOOL|DB_INIT_LOCK ;
    ok 171, my $txn = $env->txn_begin() ;
    ok 172, my $db1 = tie @array, 'BerkeleyDB::Queue', 
				-Filename => $Dfile,
				-ArrayBase => 0,
                      		-Flags    =>  DB_CREATE ,
		        	-Env 	  => $env,
		        	-Txn	  => $txn ,
				-Len      => $rec_len,
				-Pad      => " " ;

    
    # create some data
    my @data =  (
		"boat",
		"house",
		"sea",
		) ;

    my $ret = 0 ;
    my $i ;
    for ($i = 0 ; $i < @data ; ++$i) {
        $ret += $db1->db_put($i, $data[$i]) ;
    }
    ok 173, $ret == 0 ;

    # should be able to see all the records

    ok 174, my $cursor = $db1->db_cursor() ;
    my ($k, $v) = (0, "") ;
    my $count = 0 ;
    # sequence forwards
    while ($cursor->c_get($k, $v, DB_NEXT) == 0) {
        ++ $count ;
    }
    ok 175, $count == 3 ;
    undef $cursor ;

    # now abort the transaction
    ok 176, $txn->txn_abort() == 0 ;

    # there shouldn't be any records in the database
    $count = 0 ;
    # sequence forwards
    ok 177, $cursor = $db1->db_cursor() ;
    while ($cursor->c_get($k, $v, DB_NEXT) == 0) {
        ++ $count ;
    }
    ok 178, $count == 0 ;

    undef $txn ;
    undef $cursor ;
    undef $db1 ;
    undef $env ;
    untie @array ;
    rmtree $home ;
}


{
    # db_stat

    my $lex = new LexFile $Dfile ;
    my $recs = ($BerkeleyDB::db_version >= 3.1 ? "qs_ndata" : "qs_nrecs") ;
    my @array ;
    my ($k, $v) ;
    my $rec_len = 7 ;
    ok 179, my $db = new BerkeleyDB::Queue -Filename 	=> $Dfile, 
				     	   -Flags    	=> DB_CREATE,
					   -Pagesize	=> 4 * 1024,
				           -Len       => $rec_len,
				           -Pad       => " " 
					;

    my $ref = $db->db_stat() ; 
    ok 180, $ref->{$recs} == 0;
    ok 181, $ref->{'qs_pagesize'} == 4 * 1024;

    # create some data
    my @data =  (
		2,
		"house",
		"sea",
		) ;

    my $ret = 0 ;
    my $i ;
    for ($i = $db->ArrayOffset ; @data ; ++$i) {
        $ret += $db->db_put($i, shift @data) ;
    }
    ok 182, $ret == 0 ;

    $ref = $db->db_stat() ; 
    ok 183, $ref->{$recs} == 3;
}

{
   # sub-class test

   package Another ;

   use strict ;

   open(FILE, ">SubDB.pm") or die "Cannot open SubDB.pm: $!\n" ;
   print FILE <<'EOM' ;

   package SubDB ;

   use strict ;
   use vars qw( @ISA @EXPORT) ;

   require Exporter ;
   use BerkeleyDB;
   @ISA=qw(BerkeleyDB::Queue);
   @EXPORT = @BerkeleyDB::EXPORT ;

   sub db_put { 
	my $self = shift ;
        my $key = shift ;
        my $value = shift ;
        $self->SUPER::db_put($key, $value * 3) ;
   }

   sub db_get { 
	my $self = shift ;
        $self->SUPER::db_get($_[0], $_[1]) ;
	$_[1] -= 2 ;
   }

   sub A_new_method
   {
	my $self = shift ;
        my $key = shift ;
        my $value = $self->FETCH($key) ;
	return "[[$value]]" ;
   }

   1 ;
EOM

    close FILE ;

    BEGIN { push @INC, '.'; }    
    eval 'use SubDB ; ';
    main::ok 184, $@ eq "" ;
    my @h ;
    my $X ;
    my $rec_len = 34 ;
    eval '
	$X = tie(@h, "SubDB", -Filename => "dbbtree.tmp", 
			-Flags => DB_CREATE,
			-Mode => 0640 ,
	                -Len       => $rec_len,
	                -Pad       => " " 
			);		   
	' ;

    main::ok 185, $@ eq "" ;

    my $ret = eval '$h[1] = 3 ; return $h[1] ' ;
    main::ok 186, $@ eq "" ;
    main::ok 187, $ret == 7 ;

    my $value = 0;
    $ret = eval '$X->db_put(1, 4) ; $X->db_get(1, $value) ; return $value' ;
    main::ok 188, $@ eq "" ;
    main::ok 189, $ret == 10 ;

    $ret = eval ' DB_NEXT eq main::DB_NEXT ' ;
    main::ok 190, $@ eq ""  ;
    main::ok 191, $ret == 1 ;

    $ret = eval '$X->A_new_method(1) ' ;
    main::ok 192, $@ eq "" ;
    main::ok 193, $ret eq "[[10]]" ;

    unlink "SubDB.pm", "dbbtree.tmp" ;

}

{
    # DB_APPEND

    my $lex = new LexFile $Dfile;
    my @array ;
    my $value ;
    my $rec_len = 21 ;
    ok 194, my $db = tie @array, 'BerkeleyDB::Queue', 
					-Filename  => $Dfile,
                                       	-Flags     => DB_CREATE ,
	                		-Len       => $rec_len,
	                		-Pad       => " " ;

    # create a few records
    $array[1] = "def" ;
    $array[3] = "ghi" ;

    my $k = 0 ;
    ok 195, $db->db_put($k, "fred", DB_APPEND) == 0 ;
    ok 196, $k == 4 ;
    ok 197, $array[4] eq fillout("fred", $rec_len) ;

    undef $db ;
    untie @array ;
}

__END__


# TODO
#
# DB_DELIMETER DB_FIXEDLEN DB_PAD DB_SNAPSHOT with partial records
