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

print "1..7\n";

my $FA = 0 ;

{
    sub try::TIEARRAY { bless [], "try" }
    sub try::FETCHSIZE { $FA = 1 }
    $FA = 0 ;
    my @a ; 
    tie @a, 'try' ;
    my $a = @a ;
}

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

{
    package Redirect ;
    use Symbol ;

    sub new
    {
        my $class = shift ;
        my $filename = shift ;
	my $fh = gensym ;
	open ($fh, ">$filename") || die "Cannot open $filename: $!" ;
	my $real_stdout = select($fh) ;
	return bless [$fh, $real_stdout ] ;

    }
    sub DESTROY
    {
        my $self = shift ;
	close $self->[0] ;
	select($self->[1]) ;
    }
}

sub docat
{
    my $file = shift;
    local $/ = undef;
    open(CAT,$file) || die "Cannot open $file:$!";
    my $result = <CAT> || "" ;
    close(CAT);
    return $result;
}

sub docat_del
{ 
    my $file = shift;
    local $/ = undef;
    open(CAT,$file) || die "Cannot open $file: $!";
    my $result = <CAT> || "" ;
    close(CAT);
    unlink $file ;
    return $result;
}   

my $Dfile = "dbhash.tmp";
my $Dfile2 = "dbhash2.tmp";
my $Dfile3 = "dbhash3.tmp";
unlink $Dfile;

umask(0) ;

my $redirect = "xyzt" ;


{
my $x = $BerkeleyDB::Error;
my $redirect = "xyzt" ;
 {
    my $redirectObj = new Redirect $redirect ;

    use strict ;
    use BerkeleyDB ;
    use vars qw( %h $k $v ) ;
    
    my $filename = "fruit" ;
    unlink $filename ;
    tie %h, "BerkeleyDB::Hash", 
                -Filename => $filename, 
		-Flags    => DB_CREATE
        or die "Cannot open file $filename: $! $BerkeleyDB::Error\n" ;

    # Add a few key/value pairs to the file
    $h{"apple"} = "red" ;
    $h{"orange"} = "orange" ;
    $h{"banana"} = "yellow" ;
    $h{"tomato"} = "red" ;
    
    # Check for existence of a key
    print "Banana Exists\n\n" if $h{"banana"} ;
    
    # Delete a key/value pair.
    delete $h{"apple"} ;
    
    # print the contents of the file
    while (($k, $v) = each %h)
      { print "$k -> $v\n" }
      
    untie %h ;
    unlink $filename ;
 }

  #print "[" . docat($redirect) . "]" ;
  ok(1, docat_del($redirect) eq <<'EOM') ;
Banana Exists

orange -> orange
tomato -> red
banana -> yellow
EOM


}

{
my $redirect = "xyzt" ;
 {

    my $redirectObj = new Redirect $redirect ;

    use strict ;
    use BerkeleyDB ;
    
    my $filename = "fruit" ;
    unlink $filename ;
    my $db = new BerkeleyDB::Hash 
                -Filename => $filename, 
		-Flags    => DB_CREATE
        or die "Cannot open file $filename: $! $BerkeleyDB::Error\n" ;

    # Add a few key/value pairs to the file
    $db->db_put("apple", "red") ;
    $db->db_put("orange", "orange") ;
    $db->db_put("banana", "yellow") ;
    $db->db_put("tomato", "red") ;
    
    # Check for existence of a key
    print "Banana Exists\n\n" if $db->db_get("banana", $v) == 0;
    
    # Delete a key/value pair.
    $db->db_del("apple") ;
    
    # print the contents of the file
    my ($k, $v) = ("", "") ;
    my $cursor = $db->db_cursor() ;
    while ($cursor->c_get($k, $v, DB_NEXT) == 0)
      { print "$k -> $v\n" }
      
    undef $cursor ;
    undef $db ;
    unlink $filename ;
 }

  #print "[" . docat($redirect) . "]" ;
  ok(2, docat_del($redirect) eq <<'EOM') ;
Banana Exists

orange -> orange
tomato -> red
banana -> yellow
EOM

}

{
my $redirect = "xyzt" ;
 {

    my $redirectObj = new Redirect $redirect ;

    use strict ;
    use BerkeleyDB ;

    my $filename = "tree" ;
    unlink $filename ;
    my %h ;
    tie %h, 'BerkeleyDB::Btree', 
    		-Filename   => $filename, 
	        -Flags      => DB_CREATE
      or die "Cannot open $filename: $!\n" ;

    # Add a key/value pair to the file
    $h{'Wall'} = 'Larry' ;
    $h{'Smith'} = 'John' ;
    $h{'mouse'} = 'mickey' ;
    $h{'duck'}  = 'donald' ;

    # Delete
    delete $h{"duck"} ;

    # Cycle through the keys printing them in order.
    # Note it is not necessary to sort the keys as
    # the btree will have kept them in order automatically.
    foreach (keys %h)
      { print "$_\n" }

    untie %h ;
    unlink $filename ;
 }

  #print "[" . docat($redirect) . "]\n" ;
  ok(3, docat_del($redirect) eq <<'EOM') ;
Smith
Wall
mouse
EOM

}

{
my $redirect = "xyzt" ;
 {

    my $redirectObj = new Redirect $redirect ;

    use strict ;
    use BerkeleyDB ;

    my $filename = "tree" ;
    unlink $filename ;
    my %h ;
    tie %h, 'BerkeleyDB::Btree', 
    		-Filename   => $filename, 
	        -Flags      => DB_CREATE,
		-Compare    => sub { lc $_[0] cmp lc $_[1] }
      or die "Cannot open $filename: $!\n" ;

    # Add a key/value pair to the file
    $h{'Wall'} = 'Larry' ;
    $h{'Smith'} = 'John' ;
    $h{'mouse'} = 'mickey' ;
    $h{'duck'}  = 'donald' ;

    # Delete
    delete $h{"duck"} ;

    # Cycle through the keys printing them in order.
    # Note it is not necessary to sort the keys as
    # the btree will have kept them in order automatically.
    foreach (keys %h)
      { print "$_\n" }

    untie %h ;
    unlink $filename ;
 }

  #print "[" . docat($redirect) . "]\n" ;
  ok(4, docat_del($redirect) eq <<'EOM') ;
mouse
Smith
Wall
EOM

}

{
my $redirect = "xyzt" ;
 {

    my $redirectObj = new Redirect $redirect ;

    use strict ;
    use BerkeleyDB ;

    my %hash ;
    my $filename = "filt.db" ;
    unlink $filename ;

    my $db = tie %hash, 'BerkeleyDB::Hash', 
    		-Filename   => $filename, 
	        -Flags      => DB_CREATE
      or die "Cannot open $filename: $!\n" ;

    # Install DBM Filters
    $db->filter_fetch_key  ( sub { s/\0$//    } ) ;
    $db->filter_store_key  ( sub { $_ .= "\0" } ) ;
    $db->filter_fetch_value( sub { s/\0$//    } ) ;
    $db->filter_store_value( sub { $_ .= "\0" } ) ;

    $hash{"abc"} = "def" ;
    my $a = $hash{"ABC"} ;
    # ...
    undef $db ;
    untie %hash ;
    $db = tie %hash, 'BerkeleyDB::Hash', 
    		-Filename   => $filename, 
	        -Flags      => DB_CREATE
      or die "Cannot open $filename: $!\n" ;
    while (($k, $v) = each %hash)
      { print "$k -> $v\n" }
    undef $db ;
    untie %hash ;

    unlink $filename ;
 }

  #print "[" . docat($redirect) . "]\n" ;
  ok(5, docat_del($redirect) eq <<"EOM") ;
abc\x00 -> def\x00
EOM

}

{
my $redirect = "xyzt" ;
 {

    my $redirectObj = new Redirect $redirect ;

    use strict ;
    use BerkeleyDB ;
    my %hash ;
    my $filename = "filt.db" ;
    unlink $filename ;


    my $db = tie %hash, 'BerkeleyDB::Btree', 
    		-Filename   => $filename, 
	        -Flags      => DB_CREATE
      or die "Cannot open $filename: $!\n" ;

    $db->filter_fetch_key  ( sub { $_ = unpack("i", $_) } ) ;
    $db->filter_store_key  ( sub { $_ = pack ("i", $_) } ) ;
    $hash{123} = "def" ;
    # ...
    undef $db ;
    untie %hash ;
    $db = tie %hash, 'BerkeleyDB::Btree', 
    		-Filename   => $filename, 
	        -Flags      => DB_CREATE
      or die "Cannot Open $filename: $!\n" ;
    while (($k, $v) = each %hash)
      { print "$k -> $v\n" }
    undef $db ;
    untie %hash ;

    unlink $filename ;
 }

  my $val = pack("i", 123) ;
  #print "[" . docat($redirect) . "]\n" ;
  ok(6, docat_del($redirect) eq <<"EOM") ;
$val -> def
EOM

}

{
my $redirect = "xyzt" ;
 {

    my $redirectObj = new Redirect $redirect ;

    if ($FA) {
    use strict ;
    use BerkeleyDB ;

    my $filename = "text" ;
    unlink $filename ;

    my @h ;
    tie @h, 'BerkeleyDB::Recno', 
    		-Filename   => $filename, 
	        -Flags      => DB_CREATE,
		-Property   => DB_RENUMBER
      or die "Cannot open $filename: $!\n" ;

    # Add a few key/value pairs to the file
    $h[0] = "orange" ;
    $h[1] = "blue" ;
    $h[2] = "yellow" ;

    push @h, "green", "black" ;

    my $elements = scalar @h ;
    print "The array contains $elements entries\n" ;

    my $last = pop @h ;
    print "popped $last\n" ;

    unshift @h, "white" ;
    my $first = shift @h ;
    print "shifted $first\n" ;

    # Check for existence of a key
    print "Element 1 Exists with value $h[1]\n" if $h[1] ;

    untie @h ;
    unlink $filename ;
    } else {
    use strict ;
    use BerkeleyDB ;

    my $filename = "text" ;
    unlink $filename ;

    my @h ;
    my $db = tie @h, 'BerkeleyDB::Recno', 
    		-Filename   => $filename, 
	        -Flags      => DB_CREATE,
		-Property   => DB_RENUMBER
      or die "Cannot open $filename: $!\n" ;

    # Add a few key/value pairs to the file
    $h[0] = "orange" ;
    $h[1] = "blue" ;
    $h[2] = "yellow" ;

    $db->push("green", "black") ;

    my $elements = $db->length() ;
    print "The array contains $elements entries\n" ;

    my $last = $db->pop ;
    print "popped $last\n" ;

    $db->unshift("white") ;
    my $first = $db->shift ;
    print "shifted $first\n" ;

    # Check for existence of a key
    print "Element 1 Exists with value $h[1]\n" if $h[1] ;

    undef $db ;
    untie @h ;
    unlink $filename ;
    }

 }

  #print "[" . docat($redirect) . "]\n" ;
  ok(7, docat_del($redirect) eq <<"EOM") ;
The array contains 5 entries
popped black
shifted white
Element 1 Exists with value blue
EOM

}

