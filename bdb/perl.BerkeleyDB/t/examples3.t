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

BEGIN 
{
    if ($BerkeleyDB::db_version < 3) {
        print "1..0 # Skipping test, this needs Berkeley DB 3.x or better\n" ;
        exit 0 ;
    }
}


print "1..2\n";

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
my $redirect = "xyzt" ;
 {

    my $redirectObj = new Redirect $redirect ;

    use strict ;
    use BerkeleyDB ;
    
    my $filename = "fruit" ;
    unlink $filename ;
    my $db = new BerkeleyDB::Hash 
                -Filename => $filename, 
		-Flags    => DB_CREATE,
		-Property  => DB_DUP
        or die "Cannot open file $filename: $! $BerkeleyDB::Error\n" ;

    # Add a few key/value pairs to the file
    $db->db_put("red", "apple") ;
    $db->db_put("orange", "orange") ;
    $db->db_put("green", "banana") ;
    $db->db_put("yellow", "banana") ;
    $db->db_put("red", "tomato") ;
    $db->db_put("green", "apple") ;
    
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
  ok(1, docat_del($redirect) eq <<'EOM') ;
orange -> orange
yellow -> banana
red -> apple
red -> tomato
green -> banana
green -> apple
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
		-Flags    => DB_CREATE,
		-Property  => DB_DUP | DB_DUPSORT
        or die "Cannot open file $filename: $! $BerkeleyDB::Error\n" ;

    # Add a few key/value pairs to the file
    $db->db_put("red", "apple") ;
    $db->db_put("orange", "orange") ;
    $db->db_put("green", "banana") ;
    $db->db_put("yellow", "banana") ;
    $db->db_put("red", "tomato") ;
    $db->db_put("green", "apple") ;
    
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
orange -> orange
yellow -> banana
red -> apple
red -> tomato
green -> apple
green -> banana
EOM

}


