#!./perl -w

use warnings;
use strict ;

BEGIN {
    unless(grep /blib/, @INC) {
        chdir 't' if -d 't';
        @INC = '../lib' if -d '../lib';
    }
}
 
use Config;
 
BEGIN {
    if(-d "lib" && -f "TEST") {
        if ($Config{'extensions'} !~ /\bDB_File\b/ ) {
            print "1..128\n";
            exit 0;
        }
    }
}

use DB_File; 
use Fcntl;
use vars qw($dbh $Dfile $bad_ones $FA) ;

# full tied array support started in Perl 5.004_57
# Double check to see if it is available.

{
    sub try::TIEARRAY { bless [], "try" }
    sub try::FETCHSIZE { $FA = 1 }
    $FA = 0 ;
    my @a ; 
    tie @a, 'try' ;
    my $a = @a ;
}


sub ok
{
    my $no = shift ;
    my $result = shift ;

    print "not " unless $result ;
    print "ok $no\n" ;

    return $result ;
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
    my $result = <CAT>;
    close(CAT);
    return $result;
}

sub docat_del
{ 
    my $file = shift;
    local $/ = undef;
    open(CAT,$file) || die "Cannot open $file: $!";
    my $result = <CAT>;
    close(CAT);
    unlink $file ;
    return $result;
}   

sub bad_one
{
    print STDERR <<EOM unless $bad_ones++ ;
#
# Some older versions of Berkeley DB version 1 will fail tests 51,
# 53 and 55.
#
# You can safely ignore the errors if you're never going to use the
# broken functionality (recno databases with a modified bval). 
# Otherwise you'll have to upgrade your DB library.
#
# If you want to use Berkeley DB version 1, then 1.85 and 1.86 are the
# last versions that were released. Berkeley DB version 2 is continually
# being updated -- Check out http://www.sleepycat.com/ for more details.
#
EOM
}

print "1..128\n";

my $Dfile = "recno.tmp";
unlink $Dfile ;

umask(0);

# Check the interface to RECNOINFO

my $dbh = new DB_File::RECNOINFO ;
ok(1, ! defined $dbh->{bval}) ;
ok(2, ! defined $dbh->{cachesize}) ;
ok(3, ! defined $dbh->{psize}) ;
ok(4, ! defined $dbh->{flags}) ;
ok(5, ! defined $dbh->{lorder}) ;
ok(6, ! defined $dbh->{reclen}) ;
ok(7, ! defined $dbh->{bfname}) ;

$dbh->{bval} = 3000 ;
ok(8, $dbh->{bval} == 3000 );

$dbh->{cachesize} = 9000 ;
ok(9, $dbh->{cachesize} == 9000 );

$dbh->{psize} = 400 ;
ok(10, $dbh->{psize} == 400 );

$dbh->{flags} = 65 ;
ok(11, $dbh->{flags} == 65 );

$dbh->{lorder} = 123 ;
ok(12, $dbh->{lorder} == 123 );

$dbh->{reclen} = 1234 ;
ok(13, $dbh->{reclen} == 1234 );

$dbh->{bfname} = 1234 ;
ok(14, $dbh->{bfname} == 1234 );


# Check that an invalid entry is caught both for store & fetch
eval '$dbh->{fred} = 1234' ;
ok(15, $@ =~ /^DB_File::RECNOINFO::STORE - Unknown element 'fred' at/ );
eval 'my $q = $dbh->{fred}' ;
ok(16, $@ =~ /^DB_File::RECNOINFO::FETCH - Unknown element 'fred' at/ );

# Now check the interface to RECNOINFO

my $X  ;
my @h ;
ok(17, $X = tie @h, 'DB_File', $Dfile, O_RDWR|O_CREAT, 0640, $DB_RECNO ) ;

ok(18, ((stat($Dfile))[2] & 0777) == ($^O eq 'os2' ? 0666 : 0640)
	||  $^O eq 'MSWin32' || $^O eq 'amigaos') ;

#my $l = @h ;
my $l = $X->length ;
ok(19, ($FA ? @h == 0 : !$l) );

my @data = qw( a b c d ever f g h  i j k longername m n o p) ;

$h[0] = shift @data ;
ok(20, $h[0] eq 'a' );

my $ i;
foreach (@data)
  { $h[++$i] = $_ }

unshift (@data, 'a') ;

ok(21, defined $h[1] );
ok(22, ! defined $h[16] );
ok(23, $FA ? @h == @data : $X->length == @data );


# Overwrite an entry & check fetch it
$h[3] = 'replaced' ;
$data[3] = 'replaced' ;
ok(24, $h[3] eq 'replaced' );

#PUSH
my @push_data = qw(added to the end) ;
($FA ? push(@h, @push_data) : $X->push(@push_data)) ;
push (@data, @push_data) ;
ok(25, $h[++$i] eq 'added' );
ok(26, $h[++$i] eq 'to' );
ok(27, $h[++$i] eq 'the' );
ok(28, $h[++$i] eq 'end' );

# POP
my $popped = pop (@data) ;
my $value = ($FA ? pop @h : $X->pop) ;
ok(29, $value eq $popped) ;

# SHIFT
$value = ($FA ? shift @h : $X->shift) ;
my $shifted = shift @data ;
ok(30, $value eq $shifted );

# UNSHIFT

# empty list
($FA ? unshift @h : $X->unshift) ;
ok(31, ($FA ? @h == @data : $X->length == @data ));

my @new_data = qw(add this to the start of the array) ;
$FA ? unshift (@h, @new_data) : $X->unshift (@new_data) ;
unshift (@data, @new_data) ;
ok(32, $FA ? @h == @data : $X->length == @data );
ok(33, $h[0] eq "add") ;
ok(34, $h[1] eq "this") ;
ok(35, $h[2] eq "to") ;
ok(36, $h[3] eq "the") ;
ok(37, $h[4] eq "start") ;
ok(38, $h[5] eq "of") ;
ok(39, $h[6] eq "the") ;
ok(40, $h[7] eq "array") ;
ok(41, $h[8] eq $data[8]) ;

# SPLICE

# Now both arrays should be identical

my $ok = 1 ;
my $j = 0 ;
foreach (@data)
{
   $ok = 0, last if $_ ne $h[$j ++] ; 
}
ok(42, $ok );

# Neagtive subscripts

# get the last element of the array
ok(43, $h[-1] eq $data[-1] );
ok(44, $h[-1] eq $h[ ($FA ? @h : $X->length) -1] );

# get the first element using a negative subscript
eval '$h[ - ( $FA ? @h : $X->length)] = "abcd"' ;
ok(45, $@ eq "" );
ok(46, $h[0] eq "abcd" );

# now try to read before the start of the array
eval '$h[ - (1 + ($FA ? @h : $X->length))] = 1234' ;
ok(47, $@ =~ '^Modification of non-creatable array value attempted' );

# IMPORTANT - $X must be undefined before the untie otherwise the
#             underlying DB close routine will not get called.
undef $X ;
untie(@h);

unlink $Dfile;


{
    # Check bval defaults to \n

    my @h = () ;
    my $dbh = new DB_File::RECNOINFO ;
    ok(48, tie @h, 'DB_File', $Dfile, O_RDWR|O_CREAT, 0640, $dbh ) ;
    $h[0] = "abc" ;
    $h[1] = "def" ;
    $h[3] = "ghi" ;
    untie @h ;
    my $x = docat($Dfile) ;
    unlink $Dfile;
    ok(49, $x eq "abc\ndef\n\nghi\n") ;
}

{
    # Change bval

    my @h = () ;
    my $dbh = new DB_File::RECNOINFO ;
    $dbh->{bval} = "-" ;
    ok(50, tie @h, 'DB_File', $Dfile, O_RDWR|O_CREAT, 0640, $dbh ) ;
    $h[0] = "abc" ;
    $h[1] = "def" ;
    $h[3] = "ghi" ;
    untie @h ;
    my $x = docat($Dfile) ;
    unlink $Dfile;
    my $ok = ($x eq "abc-def--ghi-") ;
    bad_one() unless $ok ;
    ok(51, $ok) ;
}

{
    # Check R_FIXEDLEN with default bval (space)

    my @h = () ;
    my $dbh = new DB_File::RECNOINFO ;
    $dbh->{flags} = R_FIXEDLEN ;
    $dbh->{reclen} = 5 ;
    ok(52, tie @h, 'DB_File', $Dfile, O_RDWR|O_CREAT, 0640, $dbh ) ;
    $h[0] = "abc" ;
    $h[1] = "def" ;
    $h[3] = "ghi" ;
    untie @h ;
    my $x = docat($Dfile) ;
    unlink $Dfile;
    my $ok = ($x eq "abc  def       ghi  ") ;
    bad_one() unless $ok ;
    ok(53, $ok) ;
}

{
    # Check R_FIXEDLEN with user-defined bval

    my @h = () ;
    my $dbh = new DB_File::RECNOINFO ;
    $dbh->{flags} = R_FIXEDLEN ;
    $dbh->{bval} = "-" ;
    $dbh->{reclen} = 5 ;
    ok(54, tie @h, 'DB_File', $Dfile, O_RDWR|O_CREAT, 0640, $dbh ) ;
    $h[0] = "abc" ;
    $h[1] = "def" ;
    $h[3] = "ghi" ;
    untie @h ;
    my $x = docat($Dfile) ;
    unlink $Dfile;
    my $ok = ($x eq "abc--def-------ghi--") ;
    bad_one() unless $ok ;
    ok(55, $ok) ;
}

{
    # check that attempting to tie an associative array to a DB_RECNO will fail

    my $filename = "xyz" ;
    my %x ;
    eval { tie %x, 'DB_File', $filename, O_RDWR|O_CREAT, 0640, $DB_RECNO ; } ;
    ok(56, $@ =~ /^DB_File can only tie an array to a DB_RECNO database/) ;
    unlink $filename ;
}

{
   # sub-class test

   package Another ;

   use warnings ;
   use strict ;

   open(FILE, ">SubDB.pm") or die "Cannot open SubDB.pm: $!\n" ;
   print FILE <<'EOM' ;

   package SubDB ;

   use warnings ;
   use strict ;
   use vars qw( @ISA @EXPORT) ;

   require Exporter ;
   use DB_File;
   @ISA=qw(DB_File);
   @EXPORT = @DB_File::EXPORT ;

   sub STORE { 
	my $self = shift ;
        my $key = shift ;
        my $value = shift ;
        $self->SUPER::STORE($key, $value * 2) ;
   }

   sub FETCH { 
	my $self = shift ;
        my $key = shift ;
        $self->SUPER::FETCH($key) - 1 ;
   }

   sub put { 
	my $self = shift ;
        my $key = shift ;
        my $value = shift ;
        $self->SUPER::put($key, $value * 3) ;
   }

   sub get { 
	my $self = shift ;
        $self->SUPER::get($_[0], $_[1]) ;
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
    main::ok(57, $@ eq "") ;
    my @h ;
    my $X ;
    eval '
	$X = tie(@h, "SubDB","recno.tmp", O_RDWR|O_CREAT, 0640, $DB_RECNO );
	' ;

    main::ok(58, $@ eq "") ;

    my $ret = eval '$h[3] = 3 ; return $h[3] ' ;
    main::ok(59, $@ eq "") ;
    main::ok(60, $ret == 5) ;

    my $value = 0;
    $ret = eval '$X->put(1, 4) ; $X->get(1, $value) ; return $value' ;
    main::ok(61, $@ eq "") ;
    main::ok(62, $ret == 10) ;

    $ret = eval ' R_NEXT eq main::R_NEXT ' ;
    main::ok(63, $@ eq "" ) ;
    main::ok(64, $ret == 1) ;

    $ret = eval '$X->A_new_method(1) ' ;
    main::ok(65, $@ eq "") ;
    main::ok(66, $ret eq "[[11]]") ;

    undef $X;
    untie(@h);
    unlink "SubDB.pm", "recno.tmp" ;

}

{

    # test $#
    my $self ;
    unlink $Dfile;
    ok(67, $self = tie @h, 'DB_File', $Dfile, O_RDWR|O_CREAT, 0640, $DB_RECNO ) ;
    $h[0] = "abc" ;
    $h[1] = "def" ;
    $h[2] = "ghi" ;
    $h[3] = "jkl" ;
    ok(68, $FA ? $#h == 3 : $self->length() == 4) ;
    undef $self ;
    untie @h ;
    my $x = docat($Dfile) ;
    ok(69, $x eq "abc\ndef\nghi\njkl\n") ;

    # $# sets array to same length
    ok(70, $self = tie @h, 'DB_File', $Dfile, O_RDWR, 0640, $DB_RECNO ) ;
    if ($FA)
      { $#h = 3 }
    else 
      { $self->STORESIZE(4) }
    ok(71, $FA ? $#h == 3 : $self->length() == 4) ;
    undef $self ;
    untie @h ;
    $x = docat($Dfile) ;
    ok(72, $x eq "abc\ndef\nghi\njkl\n") ;

    # $# sets array to bigger
    ok(73, $self = tie @h, 'DB_File', $Dfile, O_RDWR, 0640, $DB_RECNO ) ;
    if ($FA)
      { $#h = 6 }
    else 
      { $self->STORESIZE(7) }
    ok(74, $FA ? $#h == 6 : $self->length() == 7) ;
    undef $self ;
    untie @h ;
    $x = docat($Dfile) ;
    ok(75, $x eq "abc\ndef\nghi\njkl\n\n\n\n") ;

    # $# sets array smaller
    ok(76, $self = tie @h, 'DB_File', $Dfile, O_RDWR, 0640, $DB_RECNO ) ;
    if ($FA)
      { $#h = 2 }
    else 
      { $self->STORESIZE(3) }
    ok(77, $FA ? $#h == 2 : $self->length() == 3) ;
    undef $self ;
    untie @h ;
    $x = docat($Dfile) ;
    ok(78, $x eq "abc\ndef\nghi\n") ;

    unlink $Dfile;


}

{
   # DBM Filter tests
   use warnings ;
   use strict ;
   my (@h, $db) ;
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
   
   ok(79, $db = tie(@h, 'DB_File', $Dfile, O_RDWR|O_CREAT, 0640, $DB_RECNO ) );

   $db->filter_fetch_key   (sub { $fetch_key = $_ }) ;
   $db->filter_store_key   (sub { $store_key = $_ }) ;
   $db->filter_fetch_value (sub { $fetch_value = $_}) ;
   $db->filter_store_value (sub { $store_value = $_ }) ;

   $_ = "original" ;

   $h[0] = "joe" ;
   #                   fk   sk     fv   sv
   ok(80, checkOutput( "", 0, "", "joe")) ;

   ($fetch_key, $store_key, $fetch_value, $store_value) = ("") x 4 ;
   ok(81, $h[0] eq "joe");
   #                   fk  sk  fv    sv
   ok(82, checkOutput( "", 0, "joe", "")) ;

   ($fetch_key, $store_key, $fetch_value, $store_value) = ("") x 4 ;
   ok(83, $db->FIRSTKEY() == 0) ;
   #                    fk     sk  fv  sv
   ok(84, checkOutput( 0, "", "", "")) ;

   # replace the filters, but remember the previous set
   my ($old_fk) = $db->filter_fetch_key   
   			(sub { ++ $_ ; $fetch_key = $_ }) ;
   my ($old_sk) = $db->filter_store_key   
   			(sub { $_ *= 2 ; $store_key = $_ }) ;
   my ($old_fv) = $db->filter_fetch_value 
   			(sub { $_ = "[$_]"; $fetch_value = $_ }) ;
   my ($old_sv) = $db->filter_store_value 
   			(sub { s/o/x/g; $store_value = $_ }) ;
   
   ($fetch_key, $store_key, $fetch_value, $store_value) = ("") x 4 ;
   $h[1] = "Joe" ;
   #                   fk   sk     fv    sv
   ok(85, checkOutput( "", 2, "", "Jxe")) ;

   ($fetch_key, $store_key, $fetch_value, $store_value) = ("") x 4 ;
   ok(86, $h[1] eq "[Jxe]");
   #                   fk   sk     fv    sv
   ok(87, checkOutput( "", 2, "[Jxe]", "")) ;

   ($fetch_key, $store_key, $fetch_value, $store_value) = ("") x 4 ;
   ok(88, $db->FIRSTKEY() == 1) ;
   #                   fk   sk     fv    sv
   ok(89, checkOutput( 1, "", "", "")) ;
   
   # put the original filters back
   $db->filter_fetch_key   ($old_fk);
   $db->filter_store_key   ($old_sk);
   $db->filter_fetch_value ($old_fv);
   $db->filter_store_value ($old_sv);

   ($fetch_key, $store_key, $fetch_value, $store_value) = ("") x 4 ;
   $h[0] = "joe" ;
   ok(90, checkOutput( "", 0, "", "joe")) ;

   ($fetch_key, $store_key, $fetch_value, $store_value) = ("") x 4 ;
   ok(91, $h[0] eq "joe");
   ok(92, checkOutput( "", 0, "joe", "")) ;

   ($fetch_key, $store_key, $fetch_value, $store_value) = ("") x 4 ;
   ok(93, $db->FIRSTKEY() == 0) ;
   ok(94, checkOutput( 0, "", "", "")) ;

   # delete the filters
   $db->filter_fetch_key   (undef);
   $db->filter_store_key   (undef);
   $db->filter_fetch_value (undef);
   $db->filter_store_value (undef);

   ($fetch_key, $store_key, $fetch_value, $store_value) = ("") x 4 ;
   $h[0] = "joe" ;
   ok(95, checkOutput( "", "", "", "")) ;

   ($fetch_key, $store_key, $fetch_value, $store_value) = ("") x 4 ;
   ok(96, $h[0] eq "joe");
   ok(97, checkOutput( "", "", "", "")) ;

   ($fetch_key, $store_key, $fetch_value, $store_value) = ("") x 4 ;
   ok(98, $db->FIRSTKEY() == 0) ;
   ok(99, checkOutput( "", "", "", "")) ;

   undef $db ;
   untie @h;
   unlink $Dfile;
}

{    
    # DBM Filter with a closure

    use warnings ;
    use strict ;
    my (@h, $db) ;

    unlink $Dfile;
    ok(100, $db = tie(@h, 'DB_File', $Dfile, O_RDWR|O_CREAT, 0640, $DB_RECNO ) );

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

    $db->filter_store_key(Closure("store key")) ;
    $db->filter_store_value(Closure("store value")) ;
    $db->filter_fetch_key(Closure("fetch key")) ;
    $db->filter_fetch_value(Closure("fetch value")) ;

    $_ = "original" ;

    $h[0] = "joe" ;
    ok(101, $result{"store key"} eq "store key - 1: [0]");
    ok(102, $result{"store value"} eq "store value - 1: [joe]");
    ok(103, ! defined $result{"fetch key"} );
    ok(104, ! defined $result{"fetch value"} );
    ok(105, $_ eq "original") ;

    ok(106, $db->FIRSTKEY() == 0 ) ;
    ok(107, $result{"store key"} eq "store key - 1: [0]");
    ok(108, $result{"store value"} eq "store value - 1: [joe]");
    ok(109, $result{"fetch key"} eq "fetch key - 1: [0]");
    ok(110, ! defined $result{"fetch value"} );
    ok(111, $_ eq "original") ;

    $h[7]  = "john" ;
    ok(112, $result{"store key"} eq "store key - 2: [0 7]");
    ok(113, $result{"store value"} eq "store value - 2: [joe john]");
    ok(114, $result{"fetch key"} eq "fetch key - 1: [0]");
    ok(115, ! defined $result{"fetch value"} );
    ok(116, $_ eq "original") ;

    ok(117, $h[0] eq "joe");
    ok(118, $result{"store key"} eq "store key - 3: [0 7 0]");
    ok(119, $result{"store value"} eq "store value - 2: [joe john]");
    ok(120, $result{"fetch key"} eq "fetch key - 1: [0]");
    ok(121, $result{"fetch value"} eq "fetch value - 1: [joe]");
    ok(122, $_ eq "original") ;

    undef $db ;
    untie @h;
    unlink $Dfile;
}		

{
   # DBM Filter recursion detection
   use warnings ;
   use strict ;
   my (@h, $db) ;
   unlink $Dfile;

   ok(123, $db = tie(@h, 'DB_File', $Dfile, O_RDWR|O_CREAT, 0640, $DB_RECNO ) );

   $db->filter_store_key (sub { $_ = $h[0] }) ;

   eval '$h[1] = 1234' ;
   ok(124, $@ =~ /^recursion detected in filter_store_key at/ );
   
   undef $db ;
   untie @h;
   unlink $Dfile;
}


{
   # Examples from the POD

  my $file = "xyzt" ;
  {
    my $redirect = new Redirect $file ;

    use warnings FATAL => qw(all);
    use strict ;
    use DB_File ;

    my $filename = "text" ;
    unlink $filename ;

    my @h ;
    my $x = tie @h, "DB_File", $filename, O_RDWR|O_CREAT, 0640, $DB_RECNO 
        or die "Cannot open file 'text': $!\n" ;

    # Add a few key/value pairs to the file
    $h[0] = "orange" ;
    $h[1] = "blue" ;
    $h[2] = "yellow" ;

    $FA ? push @h, "green", "black" 
        : $x->push("green", "black") ;

    my $elements = $FA ? scalar @h : $x->length ;
    print "The array contains $elements entries\n" ;

    my $last = $FA ? pop @h : $x->pop ;
    print "popped $last\n" ;

    $FA ? unshift @h, "white" 
        : $x->unshift("white") ;
    my $first = $FA ? shift @h : $x->shift ;
    print "shifted $first\n" ;

    # Check for existence of a key
    print "Element 1 Exists with value $h[1]\n" if $h[1] ;

    # use a negative index
    print "The last element is $h[-1]\n" ;
    print "The 2nd last element is $h[-2]\n" ;

    undef $x ;
    untie @h ;

    unlink $filename ;
  }  

  ok(125, docat_del($file) eq <<'EOM') ;
The array contains 5 entries
popped black
shifted white
Element 1 Exists with value blue
The last element is green
The 2nd last element is yellow
EOM

  my $save_output = "xyzt" ;
  {
    my $redirect = new Redirect $save_output ;

    use warnings FATAL => qw(all);
    use strict ;
    use vars qw(@h $H $file $i) ;
    use DB_File ;
    use Fcntl ;
    
    $file = "text" ;

    unlink $file ;

    $H = tie @h, "DB_File", $file, O_RDWR|O_CREAT, 0640, $DB_RECNO 
        or die "Cannot open file $file: $!\n" ;
    
    # first create a text file to play with
    $h[0] = "zero" ;
    $h[1] = "one" ;
    $h[2] = "two" ;
    $h[3] = "three" ;
    $h[4] = "four" ;

    
    # Print the records in order.
    #
    # The length method is needed here because evaluating a tied
    # array in a scalar context does not return the number of
    # elements in the array.  

    print "\nORIGINAL\n" ;
    foreach $i (0 .. $H->length - 1) {
        print "$i: $h[$i]\n" ;
    }

    # use the push & pop methods
    $a = $H->pop ;
    $H->push("last") ;
    print "\nThe last record was [$a]\n" ;

    # and the shift & unshift methods
    $a = $H->shift ;
    $H->unshift("first") ;
    print "The first record was [$a]\n" ;

    # Use the API to add a new record after record 2.
    $i = 2 ;
    $H->put($i, "Newbie", R_IAFTER) ;

    # and a new record before record 1.
    $i = 1 ;
    $H->put($i, "New One", R_IBEFORE) ;

    # delete record 3
    $H->del(3) ;

    # now print the records in reverse order
    print "\nREVERSE\n" ;
    for ($i = $H->length - 1 ; $i >= 0 ; -- $i)
      { print "$i: $h[$i]\n" }

    # same again, but use the API functions instead
    print "\nREVERSE again\n" ;
    my ($s, $k, $v)  = (0, 0, 0) ;
    for ($s = $H->seq($k, $v, R_LAST) ; 
             $s == 0 ; 
             $s = $H->seq($k, $v, R_PREV))
      { print "$k: $v\n" }

    undef $H ;
    untie @h ;    

    unlink $file ;
  }  

  ok(126, docat_del($save_output) eq <<'EOM') ;

ORIGINAL
0: zero
1: one
2: two
3: three
4: four

The last record was [four]
The first record was [zero]

REVERSE
5: last
4: three
3: Newbie
2: one
1: New One
0: first

REVERSE again
5: last
4: three
3: Newbie
2: one
1: New One
0: first
EOM
   
}

{
    # Bug ID 20001013.009
    #
    # test that $hash{KEY} = undef doesn't produce the warning
    #     Use of uninitialized value in null operation 
    use warnings ;
    use strict ;
    use DB_File ;

    unlink $Dfile;
    my @h ;
    my $a = "";
    local $SIG{__WARN__} = sub {$a = $_[0]} ;
    
    tie @h, 'DB_File', $Dfile, O_RDWR|O_CREAT, 0664, $DB_RECNO 
	or die "Can't open file: $!\n" ;
    $h[0] = undef;
    ok(127, $a eq "") ;
    untie @h ;
    unlink $Dfile;
}

{
    # test that %hash = () doesn't produce the warning
    #     Argument "" isn't numeric in entersub
    use warnings ;
    use strict ;
    use DB_File ;
    my $a = "";
    local $SIG{__WARN__} = sub {$a = $_[0]} ;

    unlink $Dfile;
    my @h ;
    
    tie @h, 'DB_File', $Dfile, O_RDWR|O_CREAT, 0664, $DB_RECNO 
	or die "Can't open file: $!\n" ;
    @h = (); ;
    ok(128, $a eq "") ;
    untie @h ;
    unlink $Dfile;
}

exit ;
