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

print "1..226\n";

my $Dfile = "dbhash.tmp";
my $Dfile2 = "dbhash2.tmp";
my $Dfile3 = "dbhash3.tmp";
unlink $Dfile;

umask(0) ;

# Check for invalid parameters
{
    # Check for invalid parameters
    my $db ;
    eval ' $db = new BerkeleyDB::Recno  -Stupid => 3 ; ' ;
    ok 1, $@ =~ /unknown key value\(s\) Stupid/  ;

    eval ' $db = new BerkeleyDB::Recno -Bad => 2, -Mode => 0345, -Stupid => 3; ' ;
    ok 2, $@ =~ /unknown key value\(s\) /  ;

    eval ' $db = new BerkeleyDB::Recno -Env => 2 ' ;
    ok 3, $@ =~ /^Env not of type BerkeleyDB::Env/ ;

    eval ' $db = new BerkeleyDB::Recno -Txn => "x" ' ;
    ok 4, $@ =~ /^Txn not of type BerkeleyDB::Txn/ ;

    my $obj = bless [], "main" ;
    eval ' $db = new BerkeleyDB::Recno -Env => $obj ' ;
    ok 5, $@ =~ /^Env not of type BerkeleyDB::Env/ ;
}

# Now check the interface to Recno

{
    my $lex = new LexFile $Dfile ;

    ok 6, my $db = new BerkeleyDB::Recno -Filename => $Dfile, 
				    -Flags    => DB_CREATE ;

    # Add a k/v pair
    my $value ;
    my $status ;
    ok 7, $db->db_put(1, "some value") == 0  ;
    ok 8, $db->status() == 0 ;
    ok 9, $db->db_get(1, $value) == 0 ;
    ok 10, $value eq "some value" ;
    ok 11, $db->db_put(2, "value") == 0  ;
    ok 12, $db->db_get(2, $value) == 0 ;
    ok 13, $value eq "value" ;
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
    ok 26, $value eq "value" ;


}


{
    # Check simple env works with a array.
    my $lex = new LexFile $Dfile ;

    my $home = "./fred" ;
    ok 27, my $lexD = new LexDir($home);

    ok 28, my $env = new BerkeleyDB::Env -Flags => DB_CREATE|DB_INIT_MPOOL,@StdErrFile,
    					 -Home => $home ;

    ok 29, my $db = new BerkeleyDB::Recno -Filename => $Dfile, 
				    -Env      => $env,
				    -Flags    => DB_CREATE ;

    # Add a k/v pair
    my $value ;
    ok 30, $db->db_put(1, "some value") == 0 ;
    ok 31, $db->db_get(1, $value) == 0 ;
    ok 32, $value eq "some value" ;
    undef $db ;
    undef $env ;
}

 
{
    # cursors

    my $lex = new LexFile $Dfile ;
    my @array ;
    my ($k, $v) ;
    ok 33, my $db = new BerkeleyDB::Recno -Filename  => $Dfile, 
				    	  -ArrayBase => 0,
				    	  -Flags     => DB_CREATE ;

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
        if ( $copy{$k} eq $v ) 
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
        if ( $copy{$k} eq $v ) 
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


    my $lex = new LexFile $Dfile ;
    my @array ;
    my $db ;
    ok 46, $db = tie @array, 'BerkeleyDB::Recno', -Filename  => $Dfile,
				    	    -Property => DB_RENUMBER,
				    	    -ArrayBase => 0,
                                            -Flags     => DB_CREATE ;

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
    ok 52, $array[1] eq "some value";
    ok 53, defined $array[1];
    ok 54, (tied @array)->status() == 0 ;
    ok 55, !defined $array[3];
    ok 56, (tied @array)->status() == DB_NOTFOUND ;

    ok 57, (tied @array)->db_del(1) == 0 ;
    ok 58, (tied @array)->status() == 0 ;
    ok 59, ! defined $array[1];
    ok 60, (tied @array)->status() == DB_NOTFOUND ;

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

    # unshift
    $FA ? unshift @array, "red", "green", "blue" 
        : $db->unshift("red", "green", "blue" ) ;
    ok 64, $array[1] eq "red" ;
    ok 65, $cursor->c_get($k, $v, DB_FIRST) == 0 ;
    ok 66, $k == 1 ;
    ok 67, $v eq "red" ;
    ok 68, $array[2] eq "green" ;
    ok 69, $cursor->c_get($k, $v, DB_NEXT) == 0 ;
    ok 70, $k == 2 ;
    ok 71, $v eq "green" ;
    ok 72, $array[3] eq "blue" ;
    ok 73, $cursor->c_get($k, $v, DB_NEXT) == 0 ;
    ok 74, $k == 3 ;
    ok 75, $v eq "blue" ;
    ok 76, $array[4] == 2 ;
    ok 77, $cursor->c_get($k, $v, DB_NEXT) == 0 ;
    ok 78, $k == 4 ;
    ok 79, $v == 2 ;

    # shift
    ok 80, ($FA ? shift @array : $db->shift()) eq "red" ;
    ok 81, ($FA ? shift @array : $db->shift()) eq "green" ;
    ok 82, ($FA ? shift @array : $db->shift()) eq "blue" ;
    ok 83, ($FA ? shift @array : $db->shift()) == 2 ;

    # push
    $FA ? push @array, "the", "end" 
        : $db->push("the", "end") ;
    ok 84, $cursor->c_get($k, $v, DB_LAST) == 0 ;
    ok 85, $k == 1001 ;
    ok 86, $v eq "end" ;
    ok 87, $cursor->c_get($k, $v, DB_PREV) == 0 ;
    ok 88, $k == 1000 ;
    ok 89, $v eq "the" ;
    ok 90, $cursor->c_get($k, $v, DB_PREV) == 0 ;
    ok 91, $k == 999 ;
    ok 92, $v == 2000 ;

    # pop
    ok 93, ( $FA ? pop @array : $db->pop ) eq "end" ;
    ok 94, ( $FA ? pop @array : $db->pop ) eq "the" ;
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
    ok 97, my $db = tie @array, 'BerkeleyDB::Recno' ;

    ok 98, $db->db_put(1, "some value") == 0  ;
    ok 99, $db->db_get(1, $value) == 0 ;
    ok 100, $value eq "some value" ;

}
 
{
    # partial
    # check works via API

    my $lex = new LexFile $Dfile ;
    my $value ;
    ok 101, my $db = new BerkeleyDB::Recno, -Filename => $Dfile,
                                      	        -Flags    => DB_CREATE ;

    # create some data
    my @data =  (
		"",
		"boat",
		"house",
		"sea",
		) ;

    my $ret = 0 ;
    my $i ;
    for ($i = 1 ; $i < @data ; ++$i) {
        $ret += $db->db_put($i, $data[$i]) ;
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
    ok 110, $db->db_get(1, $value) == 0 && $value eq "t" ;
    ok 111, $db->db_get(2, $value) == 0 && $value eq "se" ;
    ok 112, $db->db_get(3, $value) == 0 && $value eq "" ;

    # switch of partial mode
    ($pon, $off, $len) = $db->partial_clear() ;
    ok 113, $pon ;
    ok 114, $off == 3 ;
    ok 115, $len == 2 ;
    ok 116, $db->db_get(1, $value) == 0 && $value eq "boat" ;
    ok 117, $db->db_get(2, $value) == 0 && $value eq "house" ;
    ok 118, $db->db_get(3, $value) == 0 && $value eq "sea" ;

    # now partial put
    $db->partial_set(0,2) ;
    ok 119, $db->db_put(1, "") == 0 ;
    ok 120, $db->db_put(2, "AB") == 0 ;
    ok 121, $db->db_put(3, "XYZ") == 0 ;
    ok 122, $db->db_put(4, "KLM") == 0 ;

    ($pon, $off, $len) = $db->partial_clear() ;
    ok 123, $pon ;
    ok 124, $off == 0 ;
    ok 125, $len == 2 ;
    ok 126, $db->db_get(1, $value) == 0 && $value eq "at" ;
    ok 127, $db->db_get(2, $value) == 0 && $value eq "ABuse" ;
    ok 128, $db->db_get(3, $value) == 0 && $value eq "XYZa" ;
    ok 129, $db->db_get(4, $value) == 0 && $value eq "KLM" ;

    # now partial put
    ($pon, $off, $len) = $db->partial_set(3,2) ;
    ok 130, ! $pon ;
    ok 131, $off == 0 ;
    ok 132, $len == 0 ;
    ok 133, $db->db_put(1, "PPP") == 0 ;
    ok 134, $db->db_put(2, "Q") == 0 ;
    ok 135, $db->db_put(3, "XYZ") == 0 ;
    ok 136, $db->db_put(4, "TU") == 0 ;

    $db->partial_clear() ;
    ok 137, $db->db_get(1, $value) == 0 && $value eq "at\0PPP" ;
    ok 138, $db->db_get(2, $value) == 0 && $value eq "ABuQ" ;
    ok 139, $db->db_get(3, $value) == 0 && $value eq "XYZXYZ" ;
    ok 140, $db->db_get(4, $value) == 0 && $value eq "KLMTU" ;
}

{
    # partial
    # check works via tied array 

    my $lex = new LexFile $Dfile ;
    my @array ;
    my $value ;
    ok 141, my $db = tie @array, 'BerkeleyDB::Recno', -Filename => $Dfile,
                                      	        -Flags    => DB_CREATE ;

    # create some data
    my @data =  (
		"",
		"boat",
		"house",
		"sea",
		) ;

    my $i ;
    for ($i = 1 ; $i < @data ; ++$i) {
	$array[$i] = $data[$i] ;
    }


    # do a partial get
    $db->partial_set(0,2) ;
    ok 142, $array[1] eq "bo" ;
    ok 143, $array[2] eq "ho" ;
    ok 144, $array[3]  eq "se" ;

    # do a partial get, off end of data
    $db->partial_set(3,2) ;
    ok 145, $array[1] eq "t" ;
    ok 146, $array[2] eq "se" ;
    ok 147, $array[3] eq "" ;

    # switch of partial mode
    $db->partial_clear() ;
    ok 148, $array[1] eq "boat" ;
    ok 149, $array[2] eq "house" ;
    ok 150, $array[3] eq "sea" ;

    # now partial put
    $db->partial_set(0,2) ;
    ok 151, $array[1] = "" ;
    ok 152, $array[2] = "AB" ;
    ok 153, $array[3] = "XYZ" ;
    ok 154, $array[4] = "KLM" ;

    $db->partial_clear() ;
    ok 155, $array[1] eq "at" ;
    ok 156, $array[2] eq "ABuse" ;
    ok 157, $array[3] eq "XYZa" ;
    ok 158, $array[4] eq "KLM" ;

    # now partial put
    $db->partial_set(3,2) ;
    ok 159, $array[1] = "PPP" ;
    ok 160, $array[2] = "Q" ;
    ok 161, $array[3] = "XYZ" ;
    ok 162, $array[4] = "TU" ;

    $db->partial_clear() ;
    ok 163, $array[1] eq "at\0PPP" ;
    ok 164, $array[2] eq "ABuQ" ;
    ok 165, $array[3] eq "XYZXYZ" ;
    ok 166, $array[4] eq "KLMTU" ;
}

{
    # transaction

    my $lex = new LexFile $Dfile ;
    my @array ;
    my $value ;

    my $home = "./fred" ;
    ok 167, my $lexD = new LexDir($home);
    ok 168, my $env = new BerkeleyDB::Env -Home => $home,@StdErrFile,
				     -Flags => DB_CREATE|DB_INIT_TXN|
					  	DB_INIT_MPOOL|DB_INIT_LOCK ;
    ok 169, my $txn = $env->txn_begin() ;
    ok 170, my $db1 = tie @array, 'BerkeleyDB::Recno', 
				-Filename => $Dfile,
				-ArrayBase => 0,
                      		-Flags    =>  DB_CREATE ,
		        	-Env 	  => $env,
		        	-Txn	  => $txn ;

    
    ok 171, $txn->txn_commit() == 0 ;
    ok 172, $txn = $env->txn_begin() ;
    $db1->Txn($txn);

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
}


{
    # db_stat

    my $lex = new LexFile $Dfile ;
    my $recs = ($BerkeleyDB::db_version >= 3.1 ? "bt_ndata" : "bt_nrecs") ;
    my @array ;
    my ($k, $v) ;
    ok 179, my $db = new BerkeleyDB::Recno -Filename 	=> $Dfile, 
				     	   -Flags    	=> DB_CREATE,
					   -Pagesize	=> 4 * 1024,
					;

    my $ref = $db->db_stat() ; 
    ok 180, $ref->{$recs} == 0;
    ok 181, $ref->{'bt_pagesize'} == 4 * 1024;

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
   @ISA=qw(BerkeleyDB BerkeleyDB::Recno);
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
    eval '
	$X = tie(@h, "SubDB", -Filename => "dbrecno.tmp", 
			-Flags => DB_CREATE,
			-Mode => 0640 );
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

    undef $X;
    untie @h;
    unlink "SubDB.pm", "dbrecno.tmp" ;

}

{
    # variable length records, DB_DELIMETER -- defaults to \n

    my $lex = new LexFile $Dfile, $Dfile2 ;
    touch $Dfile2 ;
    my @array ;
    my $value ;
    ok 194, tie @array, 'BerkeleyDB::Recno', -Filename  => $Dfile,
						-ArrayBase => 0,
                                      	       	-Flags  => DB_CREATE ,
						-Source	=> $Dfile2 ;
    $array[0] = "abc" ;
    $array[1] = "def" ;
    $array[3] = "ghi" ;
    untie @array ;

    my $x = docat($Dfile2) ;
    ok 195, $x eq "abc\ndef\n\nghi\n" ;
}

{
    # variable length records, change DB_DELIMETER

    my $lex = new LexFile $Dfile, $Dfile2 ;
    touch $Dfile2 ;
    my @array ;
    my $value ;
    ok 196, tie @array, 'BerkeleyDB::Recno', -Filename  => $Dfile,
						-ArrayBase => 0,
                                      	       	-Flags  => DB_CREATE ,
						-Source	=> $Dfile2 ,
						-Delim	=> "-";
    $array[0] = "abc" ;
    $array[1] = "def" ;
    $array[3] = "ghi" ;
    untie @array ;

    my $x = docat($Dfile2) ;
    ok 197, $x eq "abc-def--ghi-";
}

{
    # fixed length records, default DB_PAD

    my $lex = new LexFile $Dfile, $Dfile2 ;
    touch $Dfile2 ;
    my @array ;
    my $value ;
    ok 198, tie @array, 'BerkeleyDB::Recno', -Filename  => $Dfile,
						-ArrayBase => 0,
                                      	       	-Flags  => DB_CREATE ,
						-Len 	=> 5,
						-Source	=> $Dfile2 ;
    $array[0] = "abc" ;
    $array[1] = "def" ;
    $array[3] = "ghi" ;
    untie @array ;

    my $x = docat($Dfile2) ;
    ok 199, $x eq "abc  def       ghi  " ;
}

{
    # fixed length records, change Pad

    my $lex = new LexFile $Dfile, $Dfile2 ;
    touch $Dfile2 ;
    my @array ;
    my $value ;
    ok 200, tie @array, 'BerkeleyDB::Recno', -Filename  => $Dfile,
						-ArrayBase => 0,
                                      	       	-Flags  => DB_CREATE ,
						-Len	=> 5,
						-Pad	=> "-",
						-Source	=> $Dfile2 ;
    $array[0] = "abc" ;
    $array[1] = "def" ;
    $array[3] = "ghi" ;
    untie @array ;

    my $x = docat($Dfile2) ;
    ok 201, $x eq "abc--def-------ghi--" ;
}

{
    # DB_RENUMBER

    my $lex = new LexFile $Dfile;
    my @array ;
    my $value ;
    ok 202, my $db = tie @array, 'BerkeleyDB::Recno', -Filename  => $Dfile,
				    	    	-Property => DB_RENUMBER,
						-ArrayBase => 0,
                                      	       	-Flags  => DB_CREATE ;
    # create a few records
    $array[0] = "abc" ;
    $array[1] = "def" ;
    $array[3] = "ghi" ;

    ok 203, my ($length, $joined) = joiner($db, "|") ;
    ok 204, $length == 3 ;
    ok 205, $joined eq "abc|def|ghi";

    ok 206, $db->db_del(1) == 0 ;
    ok 207, ($length, $joined) = joiner($db, "|") ;
    ok 208, $length == 2 ;
    ok 209, $joined eq "abc|ghi";

    undef $db ;
    untie @array ;

}

{
    # DB_APPEND

    my $lex = new LexFile $Dfile;
    my @array ;
    my $value ;
    ok 210, my $db = tie @array, 'BerkeleyDB::Recno', 
					-Filename  => $Dfile,
                                       	-Flags     => DB_CREATE ;

    # create a few records
    $array[1] = "def" ;
    $array[3] = "ghi" ;

    my $k = 0 ;
    ok 211, $db->db_put($k, "fred", DB_APPEND) == 0 ;
    ok 212, $k == 4 ;

    undef $db ;
    untie @array ;
}

{
    # in-memory Btree with an associated text file

    my $lex = new LexFile $Dfile2 ;
    touch $Dfile2 ;
    my @array ;
    my $value ;
    ok 213, tie @array, 'BerkeleyDB::Recno',    -Source => $Dfile2 ,
						-ArrayBase => 0,
				    	    	-Property => DB_RENUMBER,
                                      	       	-Flags  => DB_CREATE ;
    $array[0] = "abc" ;
    $array[1] = "def" ;
    $array[3] = "ghi" ;
    untie @array ;

    my $x = docat($Dfile2) ;
    ok 214, $x eq "abc\ndef\n\nghi\n" ;
}

{
    # in-memory, variable length records, change DB_DELIMETER

    my $lex = new LexFile $Dfile, $Dfile2 ;
    touch $Dfile2 ;
    my @array ;
    my $value ;
    ok 215, tie @array, 'BerkeleyDB::Recno', 
						-ArrayBase => 0,
                                      	       	-Flags  => DB_CREATE ,
						-Source	=> $Dfile2 ,
				    	    	-Property => DB_RENUMBER,
						-Delim	=> "-";
    $array[0] = "abc" ;
    $array[1] = "def" ;
    $array[3] = "ghi" ;
    untie @array ;

    my $x = docat($Dfile2) ;
    ok 216, $x eq "abc-def--ghi-";
}

{
    # in-memory, fixed length records, default DB_PAD

    my $lex = new LexFile $Dfile, $Dfile2 ;
    touch $Dfile2 ;
    my @array ;
    my $value ;
    ok 217, tie @array, 'BerkeleyDB::Recno', 	-ArrayBase => 0,
                                      	       	-Flags  => DB_CREATE ,
				    	    	-Property => DB_RENUMBER,
						-Len 	=> 5,
						-Source	=> $Dfile2 ;
    $array[0] = "abc" ;
    $array[1] = "def" ;
    $array[3] = "ghi" ;
    untie @array ;

    my $x = docat($Dfile2) ;
    ok 218, $x eq "abc  def       ghi  " ;
}

{
    # in-memory, fixed length records, change Pad

    my $lex = new LexFile $Dfile, $Dfile2 ;
    touch $Dfile2 ;
    my @array ;
    my $value ;
    ok 219, tie @array, 'BerkeleyDB::Recno', 
						-ArrayBase => 0,
                                      	       	-Flags  => DB_CREATE ,
				    	    	-Property => DB_RENUMBER,
						-Len	=> 5,
						-Pad	=> "-",
						-Source	=> $Dfile2 ;
    $array[0] = "abc" ;
    $array[1] = "def" ;
    $array[3] = "ghi" ;
    untie @array ;

    my $x = docat($Dfile2) ;
    ok 220, $x eq "abc--def-------ghi--" ;
}

{
    # 23 Sept 2001 -- push into an empty array
    my $lex = new LexFile $Dfile ;
    my @array ;
    my $db ;
    ok 221, $db = tie @array, 'BerkeleyDB::Recno', 
						-ArrayBase => 0,
                                      	       	-Flags  => DB_CREATE ,
				    	    	-Property => DB_RENUMBER,
						-Filename => $Dfile ;
    $FA ? push @array, "first"
        : $db->push("first") ;

    ok 222, $array[0] eq "first" ;
    ok 223, $FA ? pop @array : $db->pop() eq "first" ;

    undef $db;
    untie @array ;

}

{
    # 23 Sept 2001 -- unshift into an empty array
    my $lex = new LexFile $Dfile ;
    my @array ;
    my $db ;
    ok 224, $db = tie @array, 'BerkeleyDB::Recno', 
						-ArrayBase => 0,
                                      	       	-Flags  => DB_CREATE ,
				    	    	-Property => DB_RENUMBER,
						-Filename => $Dfile ;
    $FA ? unshift @array, "first"
        : $db->unshift("first") ;

    ok 225, $array[0] eq "first" ;
    ok 226, ($FA ? shift @array : $db->shift()) eq "first" ;

    undef $db;
    untie @array ;

}
__END__


# TODO
#
# DB_DELIMETER DB_FIXEDLEN DB_PAD DB_SNAPSHOT with partial records
