package util ;

package main ;

use strict ;
use BerkeleyDB ;
use File::Path qw(rmtree);
use vars qw(%DB_errors $FA) ;

$| = 1;

%DB_errors = (
    'DB_INCOMPLETE'	=> "DB_INCOMPLETE: Sync was unable to complete",
    'DB_KEYEMPTY'	=> "DB_KEYEMPTY: Non-existent key/data pair",
    'DB_KEYEXIST'	=> "DB_KEYEXIST: Key/data pair already exists",
    'DB_LOCK_DEADLOCK'  => "DB_LOCK_DEADLOCK: Locker killed to resolve a deadlock",
    'DB_LOCK_NOTGRANTED' => "DB_LOCK_NOTGRANTED: Lock not granted",
    'DB_NOTFOUND'	=> "DB_NOTFOUND: No matching key/data pair found",
    'DB_OLD_VERSION'	=> "DB_OLDVERSION: Database requires a version upgrade",
    'DB_RUNRECOVERY'	=> "DB_RUNRECOVERY: Fatal error, run database recovery",
) ;

# full tied array support started in Perl 5.004_57
# just double check.
$FA = 0 ;
{
    sub try::TIEARRAY { bless [], "try" }
    sub try::FETCHSIZE { $FA = 1 }
    my @a ; 
    tie @a, 'try' ;
    my $a = @a ;
}

{
    package LexFile ;

    use vars qw( $basename @files ) ;
    $basename = "db0000" ;

    sub new
    {
	my $self = shift ;
        #my @files = () ;
        foreach (@_)
        {
            $_ = $basename ;
            unlink $basename ;
            push @files, $basename ;
            ++ $basename ;
        }
 	bless [ @files ], $self ;
    }

    sub DESTROY
    {
	my $self = shift ;
	#unlink @{ $self } ;
    }

    END
    {
        foreach (@files) { unlink $_ }
    }
}


{
    package LexDir ;

    use File::Path qw(rmtree);

    use vars qw( $basename %dirs ) ;

    sub new
    {
        my $self = shift ;
        my $dir = shift ;
    
        rmtree $dir if -e $dir ;
    
        mkdir $dir, 0777 or return undef ;

        return bless [ $dir ], $self ;
    }
    
    sub DESTROY 
    {
        my $self = shift ;
        my $dir = $self->[0];
        #rmtree $dir;
        $dirs{$dir} ++ ;
    }

    END
    {
        foreach (keys %dirs) {
            rmtree $_ if -d $_ ;
        }
    }

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
    my $result = <CAT> || "" ;
    close(CAT);
    unlink $file ;
    return $result;
}   

sub writeFile
{
    my $name = shift ;
    open(FH, ">$name") or return 0 ;
    print FH @_ ;
    close FH ;
    return 1 ;
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

sub addData
{
    my $db = shift ;
    my @data = @_ ;
    die "addData odd data\n" if @data % 2 != 0 ;
    my ($k, $v) ;
    my $ret = 0 ;
    while (@data) {
        $k = shift @data ;
        $v = shift @data ;
        $ret += $db->db_put($k, $v) ;
    }

    return ($ret == 0) ;
}

sub ok
{
    my $no = shift ;
    my $result = shift ;
 
    print "not " unless $result ;
    print "ok $no\n" ;
}


1;
