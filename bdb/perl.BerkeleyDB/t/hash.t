#!./perl -w

# ID: %I%, %G%   

use strict ;

BEGIN {
    unless(grep /blib/, @INC) {
        chdir 't' if -d 't';
        @INC = '../lib' if -d '../lib';
    }
}

#use Config;
#
#BEGIN {
#    if(-d "lib" && -f "TEST") {
#        if ($Config{'extensions'} !~ /\bBerkeleyDB\b/ ) {
#            print "1..74\n";
#            exit 0;
#        }
#    }
#}

use BerkeleyDB; 
use File::Path qw(rmtree);

print "1..210\n";

my %DB_errors = (
    'DB_INCOMPLETE'	=> "DB_INCOMPLETE: Sync was unable to complete",
    'DB_KEYEMPTY'	=> "DB_KEYEMPTY: Non-existent key/data pair",
    'DB_KEYEXIST'	=> "DB_KEYEXIST: Key/data pair already exists",
    'DB_LOCK_DEADLOCK'  => "DB_LOCK_DEADLOCK: Locker killed to resolve a deadlock",
    'DB_LOCK_NOTGRANTED' => "DB_LOCK_NOTGRANTED: Lock not granted",
    'DB_NOTFOUND'	=> "DB_NOTFOUND: No matching key/data pair found",
    'DB_OLD_VERSION'	=> "DB_OLDVERSION: Database requires a version upgrade",
    'DB_RUNRECOVERY'	=> "DB_RUNRECOVERY: Fatal error, run database recovery",
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

my $Dfile = "dbhash.tmp";
my $Dfile2 = "dbhash2.tmp";
my $Dfile3 = "dbhash3.tmp";
unlink $Dfile;

umask(0) ;


# Check for invalid parameters
{
    # Check for invalid parameters
    my $db ;
    eval ' $db = new BerkeleyDB::Hash  -Stupid => 3 ; ' ;
    ok 1, $@ =~ /unknown key value\(s\) Stupid/  ;

    eval ' $db = new BerkeleyDB::Hash -Bad => 2, -Mode => 0345, -Stupid => 3; ' ;
    ok 2, $@ =~ /unknown key value\(s\) (Bad |Stupid ){2}/  ;

    eval ' $db = new BerkeleyDB::Hash -Env => 2 ' ;
    ok 3, $@ =~ /^Env not of type BerkeleyDB::Env/ ;

    eval ' $db = new BerkeleyDB::Hash -Txn => "fred" ' ;
    ok 4, $@ =~ /^Txn not of type BerkeleyDB::Txn/ ;

    my $obj = bless [], "main" ;
    eval ' $db = new BerkeleyDB::Hash -Env => $obj ' ;
    ok 5, $@ =~ /^Env not of type BerkeleyDB::Env/ ;
}

# Now check the interface to HASH

{
    my $lex = new LexFile $Dfile ;

    ok 6, my $db = new BerkeleyDB::Hash -Filename => $Dfile, 
				    -Flags    => DB_CREATE ;

    # Add a k/v pair
    my $value ;
    my $status ;
    ok 7, $db->db_put("some key", "some value") == 0  ;
    ok 8, $db->status() == 0 ;
    ok 9, $db->db_get("some key", $value) == 0 ;
    ok 10, $value eq "some value" ;
    ok 11, $db->db_put("key", "value") == 0  ;
    ok 12, $db->db_get("key", $value) == 0 ;
    ok 13, $value eq "value" ;
    ok 14, $db->db_del("some key") == 0 ;
    ok 15, ($status = $db->db_get("some key", $value)) == DB_NOTFOUND ;
    ok 16, $status eq $DB_errors{'DB_NOTFOUND'} ;
    ok 17, $db->status() == DB_NOTFOUND ;
    ok 18, $db->status() eq $DB_errors{'DB_NOTFOUND'};

    ok 19, $db->db_sync() == 0 ;

    # Check NOOVERWRITE will make put fail when attempting to overwrite
    # an existing record.

    ok 20, $db->db_put( 'key', 'x', DB_NOOVERWRITE) == DB_KEYEXIST ;
    ok 21, $db->status() eq $DB_errors{'DB_KEYEXIST'};
    ok 22, $db->status() == DB_KEYEXIST ;

    # check that the value of the key  has not been changed by the
    # previous test
    ok 23, $db->db_get("key", $value) == 0 ;
    ok 24, $value eq "value" ;

    # test DB_GET_BOTH
    my ($k, $v) = ("key", "value") ;
    ok 25, $db->db_get($k, $v, DB_GET_BOTH) == 0 ;

    ($k, $v) = ("key", "fred") ;
    ok 26, $db->db_get($k, $v, DB_GET_BOTH) == DB_NOTFOUND ;

    ($k, $v) = ("another", "value") ;
    ok 27, $db->db_get($k, $v, DB_GET_BOTH) == DB_NOTFOUND ;


}

{
    # Check simple env works with a hash.
    my $lex = new LexFile $Dfile ;

    my $home = "./fred" ;
    ok 28, -d $home ? chmod 0777, $home : mkdir($home, 0777) ;

    ok 29, my $env = new BerkeleyDB::Env -Flags => DB_CREATE| DB_INIT_MPOOL,
    					 -Home  => $home ;
    ok 30, my $db = new BerkeleyDB::Hash -Filename => $Dfile, 
				    -Env      => $env,
				    -Flags    => DB_CREATE ;

    # Add a k/v pair
    my $value ;
    ok 31, $db->db_put("some key", "some value") == 0 ;
    ok 32, $db->db_get("some key", $value) == 0 ;
    ok 33, $value eq "some value" ;
    undef $db ;
    undef $env ;
    rmtree $home ;
}

{
    # override default hash
    my $lex = new LexFile $Dfile ;
    my $value ;
    $::count = 0 ;
    ok 34, my $db = new BerkeleyDB::Hash -Filename => $Dfile, 
				     -Hash     => sub {  ++$::count ; length $_[0] },
				     -Flags    => DB_CREATE ;

    ok 35, $db->db_put("some key", "some value") == 0 ;
    ok 36, $db->db_get("some key", $value) == 0 ;
    ok 37, $value eq "some value" ;
    ok 38, $::count > 0 ;

}
 
{
    # cursors

    my $lex = new LexFile $Dfile ;
    my %hash ;
    my ($k, $v) ;
    ok 39, my $db = new BerkeleyDB::Hash -Filename => $Dfile, 
				     -Flags    => DB_CREATE ;

    # create some data
    my %data =  (
		"red"	=> 2,
		"green"	=> "house",
		"blue"	=> "sea",
		) ;

    my $ret = 0 ;
    while (($k, $v) = each %data) {
        $ret += $db->db_put($k, $v) ;
    }
    ok 40, $ret == 0 ;

    # create the cursor
    ok 41, my $cursor = $db->db_cursor() ;

    $k = $v = "" ;
    my %copy = %data ;
    my $extras = 0 ;
    # sequence forwards
    while ($cursor->c_get($k, $v, DB_NEXT) == 0) {
        if ( $copy{$k} eq $v ) 
            { delete $copy{$k} }
	else
	    { ++ $extras }
    }
    ok 42, $cursor->status() == DB_NOTFOUND ;
    ok 43, $cursor->status() eq $DB_errors{'DB_NOTFOUND'} ;
    ok 44, keys %copy == 0 ;
    ok 45, $extras == 0 ;

    # sequence backwards
    %copy = %data ;
    $extras = 0 ;
    my $status ;
    for ( $status = $cursor->c_get($k, $v, DB_LAST) ;
	  $status == 0 ;
    	  $status = $cursor->c_get($k, $v, DB_PREV)) {
        if ( $copy{$k} eq $v ) 
            { delete $copy{$k} }
	else
	    { ++ $extras }
    }
    ok 46, $status == DB_NOTFOUND ;
    ok 47, $status eq $DB_errors{'DB_NOTFOUND'} ;
    ok 48, $cursor->status() == $status ;
    ok 49, $cursor->status() eq $status ;
    ok 50, keys %copy == 0 ;
    ok 51, $extras == 0 ;

    ($k, $v) = ("green", "house") ;
    ok 52, $cursor->c_get($k, $v, DB_GET_BOTH) == 0 ;

    ($k, $v) = ("green", "door") ;
    ok 53, $cursor->c_get($k, $v, DB_GET_BOTH) == DB_NOTFOUND ;

    ($k, $v) = ("black", "house") ;
    ok 54, $cursor->c_get($k, $v, DB_GET_BOTH) == DB_NOTFOUND ;
    
}
 
{
    # Tied Hash interface

    my $lex = new LexFile $Dfile ;
    my %hash ;
    ok 55, tie %hash, 'BerkeleyDB::Hash', -Filename => $Dfile,
                                      -Flags    => DB_CREATE ;

    # check "each" with an empty database
    my $count = 0 ;
    while (my ($k, $v) = each %hash) {
	++ $count ;
    }
    ok 56, (tied %hash)->status() == DB_NOTFOUND ;
    ok 57, $count == 0 ;

    # Add a k/v pair
    my $value ;
    $hash{"some key"} = "some value";
    ok 58, (tied %hash)->status() == 0 ;
    ok 59, $hash{"some key"} eq "some value";
    ok 60, defined $hash{"some key"} ;
    ok 61, (tied %hash)->status() == 0 ;
    ok 62, exists $hash{"some key"} ;
    ok 63, !defined $hash{"jimmy"} ;
    ok 64, (tied %hash)->status() == DB_NOTFOUND ;
    ok 65, !exists $hash{"jimmy"} ;
    ok 66, (tied %hash)->status() == DB_NOTFOUND ;

    delete $hash{"some key"} ;
    ok 67, (tied %hash)->status() == 0 ;
    ok 68, ! defined $hash{"some key"} ;
    ok 69, (tied %hash)->status() == DB_NOTFOUND ;
    ok 70, ! exists $hash{"some key"} ;
    ok 71, (tied %hash)->status() == DB_NOTFOUND ;

    $hash{1} = 2 ;
    $hash{10} = 20 ;
    $hash{1000} = 2000 ;

    my ($keys, $values) = (0,0);
    $count = 0 ;
    while (my ($k, $v) = each %hash) {
        $keys += $k ;
	$values += $v ;
	++ $count ;
    }
    ok 72, $count == 3 ;
    ok 73, $keys == 1011 ;
    ok 74, $values == 2022 ;

    # now clear the hash
    %hash = () ;
    ok 75, keys %hash == 0 ;

    untie %hash ;
}

{
    # in-memory file

    my $lex = new LexFile $Dfile ;
    my %hash ;
    my $fd ;
    my $value ;
    ok 76, my $db = tie %hash, 'BerkeleyDB::Hash' ;

    ok 77, $db->db_put("some key", "some value") == 0  ;
    ok 78, $db->db_get("some key", $value) == 0 ;
    ok 79, $value eq "some value" ;

    undef $db ;
    untie %hash ;
}
 
{
    # partial
    # check works via API

    my $lex = new LexFile $Dfile ;
    my %hash ;
    my $value ;
    ok 80, my $db = tie %hash, 'BerkeleyDB::Hash', -Filename => $Dfile,
                                      	       -Flags    => DB_CREATE ;

    # create some data
    my %data =  (
		"red"	=> "boat",
		"green"	=> "house",
		"blue"	=> "sea",
		) ;

    my $ret = 0 ;
    while (my ($k, $v) = each %data) {
        $ret += $db->db_put($k, $v) ;
    }
    ok 81, $ret == 0 ;


    # do a partial get
    my($pon, $off, $len) = $db->partial_set(0,2) ;
    ok 82, $pon == 0 && $off == 0 && $len == 0 ;
    ok 83, ( $db->db_get("red", $value) == 0) && $value eq "bo" ;
    ok 84, ( $db->db_get("green", $value) == 0) && $value eq "ho" ;
    ok 85, ( $db->db_get("blue", $value) == 0) && $value eq "se" ;

    # do a partial get, off end of data
    ($pon, $off, $len) = $db->partial_set(3,2) ;
    ok 86, $pon ;
    ok 87, $off == 0 ;
    ok 88, $len == 2 ;
    ok 89, $db->db_get("red", $value) == 0 && $value eq "t" ;
    ok 90, $db->db_get("green", $value) == 0 && $value eq "se" ;
    ok 91, $db->db_get("blue", $value) == 0 && $value eq "" ;

    # switch of partial mode
    ($pon, $off, $len) = $db->partial_clear() ;
    ok 92, $pon ;
    ok 93, $off == 3 ;
    ok 94, $len == 2 ;
    ok 95, $db->db_get("red", $value) == 0 && $value eq "boat" ;
    ok 96, $db->db_get("green", $value) == 0 && $value eq "house" ;
    ok 97, $db->db_get("blue", $value) == 0 && $value eq "sea" ;

    # now partial put
    ($pon, $off, $len) = $db->partial_set(0,2) ;
    ok 98, ! $pon ;
    ok 99, $off == 0 ;
    ok 100, $len == 0 ;
    ok 101, $db->db_put("red", "") == 0 ;
    ok 102, $db->db_put("green", "AB") == 0 ;
    ok 103, $db->db_put("blue", "XYZ") == 0 ;
    ok 104, $db->db_put("new", "KLM") == 0 ;

    $db->partial_clear() ;
    ok 105, $db->db_get("red", $value) == 0 && $value eq "at" ;
    ok 106, $db->db_get("green", $value) == 0 && $value eq "ABuse" ;
    ok 107, $db->db_get("blue", $value) == 0 && $value eq "XYZa" ;
    ok 108, $db->db_get("new", $value) == 0 && $value eq "KLM" ;

    # now partial put
    $db->partial_set(3,2) ;
    ok 109, $db->db_put("red", "PPP") == 0 ;
    ok 110, $db->db_put("green", "Q") == 0 ;
    ok 111, $db->db_put("blue", "XYZ") == 0 ;
    ok 112, $db->db_put("new", "--") == 0 ;

    ($pon, $off, $len) = $db->partial_clear() ;
    ok 113, $pon ;
    ok 114, $off == 3 ;
    ok 115, $len == 2 ;
    ok 116, $db->db_get("red", $value) == 0 && $value eq "at\0PPP" ;
    ok 117, $db->db_get("green", $value) == 0 && $value eq "ABuQ" ;
    ok 118, $db->db_get("blue", $value) == 0 && $value eq "XYZXYZ" ;
    ok 119, $db->db_get("new", $value) == 0 && $value eq "KLM--" ;
}

{
    # partial
    # check works via tied hash 

    my $lex = new LexFile $Dfile ;
    my %hash ;
    my $value ;
    ok 120, my $db = tie %hash, 'BerkeleyDB::Hash', -Filename => $Dfile,
                                      	       -Flags    => DB_CREATE ;

    # create some data
    my %data =  (
		"red"	=> "boat",
		"green"	=> "house",
		"blue"	=> "sea",
		) ;

    while (my ($k, $v) = each %data) {
	$hash{$k} = $v ;
    }


    # do a partial get
    $db->partial_set(0,2) ;
    ok 121, $hash{"red"} eq "bo" ;
    ok 122, $hash{"green"} eq "ho" ;
    ok 123, $hash{"blue"}  eq "se" ;

    # do a partial get, off end of data
    $db->partial_set(3,2) ;
    ok 124, $hash{"red"} eq "t" ;
    ok 125, $hash{"green"} eq "se" ;
    ok 126, $hash{"blue"} eq "" ;

    # switch of partial mode
    $db->partial_clear() ;
    ok 127, $hash{"red"} eq "boat" ;
    ok 128, $hash{"green"} eq "house" ;
    ok 129, $hash{"blue"} eq "sea" ;

    # now partial put
    $db->partial_set(0,2) ;
    ok 130, $hash{"red"} = "" ;
    ok 131, $hash{"green"} = "AB" ;
    ok 132, $hash{"blue"} = "XYZ" ;
    ok 133, $hash{"new"} = "KLM" ;

    $db->partial_clear() ;
    ok 134, $hash{"red"} eq "at" ;
    ok 135, $hash{"green"} eq "ABuse" ;
    ok 136, $hash{"blue"} eq "XYZa" ;
    ok 137, $hash{"new"} eq "KLM" ;

    # now partial put
    $db->partial_set(3,2) ;
    ok 138, $hash{"red"} = "PPP" ;
    ok 139, $hash{"green"} = "Q" ;
    ok 140, $hash{"blue"} = "XYZ" ;
    ok 141, $hash{"new"} = "TU" ;

    $db->partial_clear() ;
    ok 142, $hash{"red"} eq "at\0PPP" ;
    ok 143, $hash{"green"} eq "ABuQ" ;
    ok 144, $hash{"blue"} eq "XYZXYZ" ;
    ok 145, $hash{"new"} eq "KLMTU" ;
}

{
    # transaction

    my $lex = new LexFile $Dfile ;
    my %hash ;
    my $value ;

    my $home = "./fred" ;
    rmtree $home if -e $home ;
    ok 146, mkdir($home, 0777) ;
    ok 147, my $env = new BerkeleyDB::Env -Home => $home,
				     -Flags => DB_CREATE|DB_INIT_TXN|
					  	DB_INIT_MPOOL|DB_INIT_LOCK ;
    ok 148, my $txn = $env->txn_begin() ;
    ok 149, my $db1 = tie %hash, 'BerkeleyDB::Hash', -Filename => $Dfile,
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
    ok 150, $ret == 0 ;

    # should be able to see all the records

    ok 151, my $cursor = $db1->db_cursor() ;
    my ($k, $v) = ("", "") ;
    my $count = 0 ;
    # sequence forwards
    while ($cursor->c_get($k, $v, DB_NEXT) == 0) {
        ++ $count ;
    }
    ok 152, $count == 3 ;
    undef $cursor ;

    # now abort the transaction
    ok 153, $txn->txn_abort() == 0 ;

    # there shouldn't be any records in the database
    $count = 0 ;
    # sequence forwards
    ok 154, $cursor = $db1->db_cursor() ;
    while ($cursor->c_get($k, $v, DB_NEXT) == 0) {
        ++ $count ;
    }
    ok 155, $count == 0 ;

    undef $txn ;
    undef $cursor ;
    undef $db1 ;
    undef $env ;
    untie %hash ;
    rmtree $home ;
}


{
    # DB_DUP

    my $lex = new LexFile $Dfile ;
    my %hash ;
    ok 156, my $db = tie %hash, 'BerkeleyDB::Hash', -Filename => $Dfile,
				      -Property  => DB_DUP,
                                      -Flags    => DB_CREATE ;

    $hash{'Wall'} = 'Larry' ;
    $hash{'Wall'} = 'Stone' ;
    $hash{'Smith'} = 'John' ;
    $hash{'Wall'} = 'Brick' ;
    $hash{'Wall'} = 'Brick' ;
    $hash{'mouse'} = 'mickey' ;

    ok 157, keys %hash == 6 ;

    # create a cursor
    ok 158, my $cursor = $db->db_cursor() ;

    my $key = "Wall" ;
    my $value ;
    ok 159, $cursor->c_get($key, $value, DB_SET) == 0 ;
    ok 160, $key eq "Wall" && $value eq "Larry" ;
    ok 161, $cursor->c_get($key, $value, DB_NEXT) == 0 ;
    ok 162, $key eq "Wall" && $value eq "Stone" ;
    ok 163, $cursor->c_get($key, $value, DB_NEXT) == 0 ;
    ok 164, $key eq "Wall" && $value eq "Brick" ;
    ok 165, $cursor->c_get($key, $value, DB_NEXT) == 0 ;
    ok 166, $key eq "Wall" && $value eq "Brick" ;

    #my $ref = $db->db_stat() ; 
    #ok 143, $ref->{bt_flags} | DB_DUP ;

    # test DB_DUP_NEXT
    my ($k, $v) = ("Wall", "") ;
    ok 167, $cursor->c_get($k, $v, DB_SET) == 0 ;
    ok 168, $k eq "Wall" && $v eq "Larry" ;
    ok 169, $cursor->c_get($k, $v, DB_NEXT_DUP) == 0 ;
    ok 170, $k eq "Wall" && $v eq "Stone" ;
    ok 171, $cursor->c_get($k, $v, DB_NEXT_DUP) == 0 ;
    ok 172, $k eq "Wall" && $v eq "Brick" ;
    ok 173, $cursor->c_get($k, $v, DB_NEXT_DUP) == 0 ;
    ok 174, $k eq "Wall" && $v eq "Brick" ;
    ok 175, $cursor->c_get($k, $v, DB_NEXT_DUP) == DB_NOTFOUND ;
    

    undef $db ;
    undef $cursor ;
    untie %hash ;

}

{
    # DB_DUP & DupCompare
    my $lex = new LexFile $Dfile, $Dfile2;
    my ($key, $value) ;
    my (%h, %g) ;
    my @Keys   = qw( 0123 9 12 -1234 9 987654321 9 def  ) ; 
    my @Values = qw( 1    11 3   dd   x abc      2 0    ) ; 

    ok 176, tie %h, "BerkeleyDB::Hash", -Filename => $Dfile, 
				     -DupCompare   => sub { $_[0] cmp $_[1] },
				     -Property  => DB_DUP|DB_DUPSORT,
				     -Flags    => DB_CREATE ;

    ok 177, tie %g, 'BerkeleyDB::Hash', -Filename => $Dfile2, 
				     -DupCompare   => sub { $_[0] <=> $_[1] },
				     -Property  => DB_DUP|DB_DUPSORT,
				     -Flags    => DB_CREATE ;

    foreach (@Keys) {
        local $^W = 0 ;
	my $value = shift @Values ;
        $h{$_} = $value ; 
        $g{$_} = $value ;
    }

    ok 178, my $cursor = (tied %h)->db_cursor() ;
    $key = 9 ; $value = "";
    ok 179, $cursor->c_get($key, $value, DB_SET) == 0 ;
    ok 180, $key == 9 && $value eq 11 ;
    ok 181, $cursor->c_get($key, $value, DB_NEXT) == 0 ;
    ok 182, $key == 9 && $value == 2 ;
    ok 183, $cursor->c_get($key, $value, DB_NEXT) == 0 ;
    ok 184, $key == 9 && $value eq "x" ;

    $cursor = (tied %g)->db_cursor() ;
    $key = 9 ;
    ok 185, $cursor->c_get($key, $value, DB_SET) == 0 ;
    ok 186, $key == 9 && $value eq "x" ;
    ok 187, $cursor->c_get($key, $value, DB_NEXT) == 0 ;
    ok 188, $key == 9 && $value == 2 ;
    ok 189, $cursor->c_get($key, $value, DB_NEXT) == 0 ;
    ok 190, $key == 9 && $value  == 11 ;


}

{
    # get_dup etc
    my $lex = new LexFile $Dfile;
    my %hh ;

    ok 191, my $YY = tie %hh, "BerkeleyDB::Hash", -Filename => $Dfile, 
				     -DupCompare   => sub { $_[0] cmp $_[1] },
				     -Property  => DB_DUP,
				     -Flags    => DB_CREATE ;

    $hh{'Wall'} = 'Larry' ;
    $hh{'Wall'} = 'Stone' ; # Note the duplicate key
    $hh{'Wall'} = 'Brick' ; # Note the duplicate key
    $hh{'Smith'} = 'John' ;
    $hh{'mouse'} = 'mickey' ;
    
    # first work in scalar context
    ok 192, scalar $YY->get_dup('Unknown') == 0 ;
    ok 193, scalar $YY->get_dup('Smith') == 1 ;
    ok 194, scalar $YY->get_dup('Wall') == 3 ;
    
    # now in list context
    my @unknown = $YY->get_dup('Unknown') ;
    ok 195, "@unknown" eq "" ;
    
    my @smith = $YY->get_dup('Smith') ;
    ok 196, "@smith" eq "John" ;
    
    {
        my @wall = $YY->get_dup('Wall') ;
        my %wall ;
        @wall{@wall} = @wall ;
        ok 197, (@wall == 3 && $wall{'Larry'} 
			&& $wall{'Stone'} && $wall{'Brick'});
    }
    
    # hash
    my %unknown = $YY->get_dup('Unknown', 1) ;
    ok 198, keys %unknown == 0 ;
    
    my %smith = $YY->get_dup('Smith', 1) ;
    ok 199, keys %smith == 1 && $smith{'John'} ;
    
    my %wall = $YY->get_dup('Wall', 1) ;
    ok 200, keys %wall == 3 && $wall{'Larry'} == 1 && $wall{'Stone'} == 1 
    		&& $wall{'Brick'} == 1 ;
    
    undef $YY ;
    untie %hh ;

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
   @ISA=qw(BerkeleyDB::Hash);
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
    main::ok 201, $@ eq "" ;
    my %h ;
    my $X ;
    eval '
	$X = tie(%h, "SubDB", -Filename => "dbhash.tmp", 
			-Flags => DB_CREATE,
			-Mode => 0640 );
	' ;

    main::ok 202, $@ eq "" ;

    my $ret = eval '$h{"fred"} = 3 ; return $h{"fred"} ' ;
    main::ok 203, $@ eq "" ;
    main::ok 204, $ret == 7 ;

    my $value = 0;
    $ret = eval '$X->db_put("joe", 4) ; $X->db_get("joe", $value) ; return $value' ;
    main::ok 205, $@ eq "" ;
    main::ok 206, $ret == 10 ;

    $ret = eval ' DB_NEXT eq main::DB_NEXT ' ;
    main::ok 207, $@ eq ""  ;
    main::ok 208, $ret == 1 ;

    $ret = eval '$X->A_new_method("joe") ' ;
    main::ok 209, $@ eq "" ;
    main::ok 210, $ret eq "[[10]]" ;

    unlink "SubDB.pm", "dbhash.tmp" ;

}
