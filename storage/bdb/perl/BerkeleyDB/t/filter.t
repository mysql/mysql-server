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

print "1..46\n";

my $Dfile = "dbhash.tmp";
unlink $Dfile;

umask(0) ;


{
   # DBM Filter tests
   use strict ;
   my (%h, $db) ;
   my ($fetch_key, $store_key, $fetch_value, $store_value) = ("") x 4 ;
   unlink $Dfile;

   sub checkOutput
   {
       my($fk, $sk, $fv, $sv) = @_ ;
       return
           $fetch_key eq $fk && $store_key eq $sk && 
	   $fetch_value eq $fv && $store_value eq $sv &&
	   $_ eq 'original' ;
   }
   
    ok 1, $db = tie %h, 'BerkeleyDB::Hash', 
    		-Filename   => $Dfile, 
	        -Flags      => DB_CREATE; 

   $db->filter_fetch_key   (sub { $fetch_key = $_ }) ;
   $db->filter_store_key   (sub { $store_key = $_ }) ;
   $db->filter_fetch_value (sub { $fetch_value = $_}) ;
   $db->filter_store_value (sub { $store_value = $_ }) ;

   $_ = "original" ;

   $h{"fred"} = "joe" ;
   #                   fk   sk     fv   sv
   ok 2, checkOutput( "", "fred", "", "joe") ;

   ($fetch_key, $store_key, $fetch_value, $store_value) = ("") x 4 ;
   ok 3, $h{"fred"} eq "joe";
   #                   fk    sk     fv    sv
   ok 4, checkOutput( "", "fred", "joe", "") ;

   ($fetch_key, $store_key, $fetch_value, $store_value) = ("") x 4 ;
   ok 5, $db->FIRSTKEY() eq "fred" ;
   #                    fk     sk  fv  sv
   ok 6, checkOutput( "fred", "", "", "") ;

   # replace the filters, but remember the previous set
   my ($old_fk) = $db->filter_fetch_key   
   			(sub { $_ = uc $_ ; $fetch_key = $_ }) ;
   my ($old_sk) = $db->filter_store_key   
   			(sub { $_ = lc $_ ; $store_key = $_ }) ;
   my ($old_fv) = $db->filter_fetch_value 
   			(sub { $_ = "[$_]"; $fetch_value = $_ }) ;
   my ($old_sv) = $db->filter_store_value 
   			(sub { s/o/x/g; $store_value = $_ }) ;
   
   ($fetch_key, $store_key, $fetch_value, $store_value) = ("") x 4 ;
   $h{"Fred"} = "Joe" ;
   #                   fk   sk     fv    sv
   ok 7, checkOutput( "", "fred", "", "Jxe") ;

   ($fetch_key, $store_key, $fetch_value, $store_value) = ("") x 4 ;
   ok 8, $h{"Fred"} eq "[Jxe]";
   #                   fk   sk     fv    sv
   ok 9, checkOutput( "", "fred", "[Jxe]", "") ;

   ($fetch_key, $store_key, $fetch_value, $store_value) = ("") x 4 ;
   ok 10, $db->FIRSTKEY() eq "FRED" ;
   #                   fk   sk     fv    sv
   ok 11, checkOutput( "FRED", "", "", "") ;

   # put the original filters back
   $db->filter_fetch_key   ($old_fk);
   $db->filter_store_key   ($old_sk);
   $db->filter_fetch_value ($old_fv);
   $db->filter_store_value ($old_sv);

   ($fetch_key, $store_key, $fetch_value, $store_value) = ("") x 4 ;
   $h{"fred"} = "joe" ;
   ok 12, checkOutput( "", "fred", "", "joe") ;

   ($fetch_key, $store_key, $fetch_value, $store_value) = ("") x 4 ;
   ok 13, $h{"fred"} eq "joe";
   ok 14, checkOutput( "", "fred", "joe", "") ;

   ($fetch_key, $store_key, $fetch_value, $store_value) = ("") x 4 ;
   ok 15, $db->FIRSTKEY() eq "fred" ;
   ok 16, checkOutput( "fred", "", "", "") ;

   # delete the filters
   $db->filter_fetch_key   (undef);
   $db->filter_store_key   (undef);
   $db->filter_fetch_value (undef);
   $db->filter_store_value (undef);

   ($fetch_key, $store_key, $fetch_value, $store_value) = ("") x 4 ;
   $h{"fred"} = "joe" ;
   ok 17, checkOutput( "", "", "", "") ;

   ($fetch_key, $store_key, $fetch_value, $store_value) = ("") x 4 ;
   ok 18, $h{"fred"} eq "joe";
   ok 19, checkOutput( "", "", "", "") ;

   ($fetch_key, $store_key, $fetch_value, $store_value) = ("") x 4 ;
   ok 20, $db->FIRSTKEY() eq "fred" ;
   ok 21, checkOutput( "", "", "", "") ;

   undef $db ;
   untie %h;
   unlink $Dfile;
}

{    
    # DBM Filter with a closure

    use strict ;
    my (%h, $db) ;

    unlink $Dfile;
    ok 22, $db = tie %h, 'BerkeleyDB::Hash', 
    		-Filename   => $Dfile, 
	        -Flags      => DB_CREATE; 

    my %result = () ;

    sub Closure
    {
        my ($name) = @_ ;
	my $count = 0 ;
	my @kept = () ;

	return sub { ++$count ; 
		     push @kept, $_ ; 
		     $result{$name} = "$name - $count: [@kept]" ;
		   }
    }

    $db->filter_store_key(Closure("store key"))  ;
    $db->filter_store_value(Closure("store value")) ;
    $db->filter_fetch_key(Closure("fetch key")) ;
    $db->filter_fetch_value(Closure("fetch value")) ;

    $_ = "original" ;

    $h{"fred"} = "joe" ;
    ok 23, $result{"store key"} eq "store key - 1: [fred]" ;
    ok 24, $result{"store value"} eq "store value - 1: [joe]" ;
    ok 25, ! defined $result{"fetch key"}  ;
    ok 26, ! defined $result{"fetch value"}  ;
    ok 27, $_ eq "original"  ;

    ok 28, $db->FIRSTKEY() eq "fred"  ;
    ok 29, $result{"store key"} eq "store key - 1: [fred]" ;
    ok 30, $result{"store value"} eq "store value - 1: [joe]" ;
    ok 31, $result{"fetch key"} eq "fetch key - 1: [fred]" ;
    ok 32, ! defined $result{"fetch value"}  ;
    ok 33, $_ eq "original"  ;

    $h{"jim"}  = "john" ;
    ok 34, $result{"store key"} eq "store key - 2: [fred jim]" ;
    ok 35, $result{"store value"} eq "store value - 2: [joe john]" ;
    ok 36, $result{"fetch key"} eq "fetch key - 1: [fred]" ;
    ok 37, ! defined $result{"fetch value"}  ;
    ok 38, $_ eq "original"  ;

    ok 39, $h{"fred"} eq "joe" ;
    ok 40, $result{"store key"} eq "store key - 3: [fred jim fred]" ;
    ok 41, $result{"store value"} eq "store value - 2: [joe john]" ;
    ok 42, $result{"fetch key"} eq "fetch key - 1: [fred]" ;
    ok 43, $result{"fetch value"} eq "fetch value - 1: [joe]" ;
    ok 44, $_ eq "original" ;

    undef $db ;
    untie %h;
    unlink $Dfile;
}		

{
   # DBM Filter recursion detection
   use strict ;
   my (%h, $db) ;
   unlink $Dfile;

    ok 45, $db = tie %h, 'BerkeleyDB::Hash', 
    		-Filename   => $Dfile, 
	        -Flags      => DB_CREATE; 

   $db->filter_store_key (sub { $_ = $h{$_} }) ;

   eval '$h{1} = 1234' ;
   ok 46, $@ =~ /^BerkeleyDB Aborting: recursion detected in filter_store_key at/ ;
   #print "[$@]\n" ;
   
   undef $db ;
   untie %h;
   unlink $Dfile;
}

