
package BerkeleyDB;


#     Copyright (c) 1997-2001 Paul Marquess. All rights reserved.
#     This program is free software; you can redistribute it and/or
#     modify it under the same terms as Perl itself.
#

# The documentation for this module is at the bottom of this file,
# after the line __END__.

BEGIN { require 5.004_04 }

use strict;
use Carp;
use vars qw($VERSION @ISA @EXPORT $AUTOLOAD);

$VERSION = '0.13';

require Exporter;
require DynaLoader;
require AutoLoader;
use IO ;

@ISA = qw(Exporter DynaLoader);
# Items to export into callers namespace by default. Note: do not export
# names by default without a very good reason. Use EXPORT_OK instead.
# Do not simply export all your public functions/methods/constants.
@EXPORT = qw(

	DB_AFTER
	DB_APPEND
	DB_ARCH_ABS
	DB_ARCH_DATA
	DB_ARCH_LOG
	DB_BEFORE
	DB_BTREE
	DB_BTREEMAGIC
	DB_BTREEOLDVER
	DB_BTREEVERSION
	DB_CHECKPOINT
	DB_CONSUME
	DB_CREATE
	DB_CURLSN
	DB_CURRENT
	DB_DBT_MALLOC
	DB_DBT_PARTIAL
	DB_DBT_USERMEM
	DB_DELETED
	DB_DELIMITER
	DB_DUP
	DB_DUPSORT
	DB_ENV_APPINIT
	DB_ENV_STANDALONE
	DB_ENV_THREAD
	DB_EXCL
	DB_FILE_ID_LEN
	DB_FIRST
	DB_FIXEDLEN
	DB_FLUSH
	DB_FORCE
	DB_GET_BOTH
	DB_GET_RECNO
	DB_HASH
	DB_HASHMAGIC
	DB_HASHOLDVER
	DB_HASHVERSION
	DB_INCOMPLETE
	DB_INIT_CDB
	DB_INIT_LOCK
	DB_INIT_LOG
	DB_INIT_MPOOL
	DB_INIT_TXN
	DB_JOIN_ITEM
	DB_KEYEMPTY
	DB_KEYEXIST
	DB_KEYFIRST
	DB_KEYLAST
	DB_LAST
	DB_LOCKMAGIC
	DB_LOCKVERSION
	DB_LOCK_CONFLICT
	DB_LOCK_DEADLOCK
	DB_LOCK_DEFAULT
	DB_LOCK_GET
	DB_LOCK_NORUN
	DB_LOCK_NOTGRANTED
	DB_LOCK_NOTHELD
	DB_LOCK_NOWAIT
	DB_LOCK_OLDEST
	DB_LOCK_RANDOM
	DB_LOCK_RIW_N
	DB_LOCK_RW_N
	DB_LOCK_YOUNGEST
	DB_LOGMAGIC
	DB_LOGOLDVER
	DB_MAX_PAGES
	DB_MAX_RECORDS
	DB_MPOOL_CLEAN
	DB_MPOOL_CREATE
	DB_MPOOL_DIRTY
	DB_MPOOL_DISCARD
	DB_MPOOL_LAST
	DB_MPOOL_NEW
	DB_MPOOL_PRIVATE
	DB_MUTEXDEBUG
	DB_MUTEXLOCKS
	DB_NEEDSPLIT
	DB_NEXT
	DB_NEXT_DUP
	DB_NOMMAP
	DB_NOOVERWRITE
	DB_NOSYNC
	DB_NOTFOUND
	DB_PAD
	DB_PAGEYIELD
	DB_POSITION
	DB_PREV
	DB_PRIVATE
	DB_QUEUE
	DB_RDONLY
	DB_RECNO
	DB_RECNUM
	DB_RECORDCOUNT
	DB_RECOVER
	DB_RECOVER_FATAL
	DB_REGISTERED
	DB_RENUMBER
	DB_RMW
	DB_RUNRECOVERY
	DB_SEQUENTIAL
	DB_SET
	DB_SET_RANGE
	DB_SET_RECNO
	DB_SNAPSHOT
	DB_SWAPBYTES
	DB_TEMPORARY
	DB_THREAD
	DB_TRUNCATE
	DB_TXNMAGIC
	DB_TXNVERSION
	DB_TXN_BACKWARD_ROLL
	DB_TXN_CKP
	DB_TXN_FORWARD_ROLL
	DB_TXN_LOCK_2PL
	DB_TXN_LOCK_MASK
	DB_TXN_LOCK_OPTIMIST
	DB_TXN_LOCK_OPTIMISTIC
	DB_TXN_LOG_MASK
	DB_TXN_LOG_REDO
	DB_TXN_LOG_UNDO
	DB_TXN_LOG_UNDOREDO
	DB_TXN_NOSYNC
	DB_TXN_NOWAIT
	DB_TXN_OPENFILES
	DB_TXN_REDO
	DB_TXN_SYNC
	DB_TXN_UNDO
	DB_USE_ENVIRON
	DB_USE_ENVIRON_ROOT
	DB_VERSION_MAJOR
	DB_VERSION_MINOR
	DB_VERSION_PATCH
	DB_WRITECURSOR
	);

sub AUTOLOAD {
    # This AUTOLOAD is used to 'autoload' constants from the constant()
    # XS function.  If a constant is not found then control is passed
    # to the AUTOLOAD in AutoLoader.

    my $constname;
    ($constname = $AUTOLOAD) =~ s/.*:://;
    my $val = constant($constname, @_ ? $_[0] : 0);
    if ($! != 0) {
	if ($! =~ /Invalid/) {
	    $AutoLoader::AUTOLOAD = $AUTOLOAD;
	    goto &AutoLoader::AUTOLOAD;
	}
	else {
		croak "Your vendor has not defined BerkeleyDB macro $constname";
	}
    }
    eval "sub $AUTOLOAD { $val }";
    goto &$AUTOLOAD;
}

bootstrap BerkeleyDB $VERSION;

# Preloaded methods go here.


sub ParseParameters($@)
{
    my ($default, @rest) = @_ ;
    my (%got) = %$default ;
    my (@Bad) ;
    my ($key, $value) ;
    my $sub = (caller(1))[3] ;
    my %options = () ;
    local ($Carp::CarpLevel) = 1 ;

    # allow the options to be passed as a hash reference or
    # as the complete hash.
    if (@rest == 1) {

        croak "$sub: parameter is not a reference to a hash"
            if ref $rest[0] ne "HASH" ;

        %options = %{ $rest[0] } ;
    }
    elsif (@rest >= 2) {
        %options = @rest ;
    }

    while (($key, $value) = each %options)
    {
	$key =~ s/^-// ;

        if (exists $default->{$key})
          { $got{$key} = $value }
        else
	  { push (@Bad, $key) }
    }
    
    if (@Bad) {
        my ($bad) = join(", ", @Bad) ;
        croak "unknown key value(s) @Bad" ;
    }

    return \%got ;
}

use UNIVERSAL qw( isa ) ;

sub env_remove
{
    # Usage:
    #
    #	$env = new BerkeleyDB::Env
    #			[ -Home		=> $path, ]
    #			[ -Config	=> { name => value, name => value }
    #			[ -Flags	=> DB_INIT_LOCK| ]
    #			;

    my $got = BerkeleyDB::ParseParameters({
					Home		=> undef,
					Flags     	=> 0,
					Config		=> undef,
					}, @_) ;

    if (defined $got->{ErrFile}) {
	if (!isaFilehandle($got->{ErrFile})) {
	    my $handle = new IO::File ">$got->{ErrFile}"
		or croak "Cannot open file $got->{ErrFile}: $!\n" ;
	    $got->{ErrFile} = $handle ;
	}
    }

    
    if (defined $got->{Config}) {
    	croak("Config parameter must be a hash reference")
            if ! ref $got->{Config} eq 'HASH' ;

        @BerkeleyDB::a = () ;
	my $k = "" ; my $v = "" ;
	while (($k, $v) = each %{$got->{Config}}) {
	    push @BerkeleyDB::a, "$k\t$v" ;
	}

        $got->{"Config"} = pack("p*", @BerkeleyDB::a, undef) 
	    if @BerkeleyDB::a ;
    }

    return _env_remove($got) ;
}

sub db_remove
{
    my $got = BerkeleyDB::ParseParameters(
		      {
			Filename 	=> undef,
			Subname		=> undef,
			Flags		=> 0,
			Env		=> undef,
		      }, @_) ;

    croak("Must specify a filename")
	if ! defined $got->{Filename} ;

    croak("Env not of type BerkeleyDB::Env")
	if defined $got->{Env} and ! isa($got->{Env},'BerkeleyDB::Env');

    return _db_remove($got);
}

package BerkeleyDB::Env ;

use UNIVERSAL qw( isa ) ;
use Carp ;
use vars qw( %valid_config_keys ) ;

sub isaFilehandle
{
    my $fh = shift ;

    return ((isa($fh,'GLOB') or isa(\$fh,'GLOB')) and defined fileno($fh) )

}

%valid_config_keys = map { $_, 1 } qw( DB_DATA_DIR DB_LOG_DIR DB_TEMP_DIR ) ;

sub new
{
    # Usage:
    #
    #	$env = new BerkeleyDB::Env
    #			[ -Home		=> $path, ]
    #			[ -Mode		=> mode, ]
    #			[ -Config	=> { name => value, name => value }
    #			[ -ErrFile   	=> filename or filehandle, ]
    #			[ -ErrPrefix 	=> "string", ]
    #			[ -Flags	=> DB_INIT_LOCK| ]
    #			[ -Cachesize	=> number ]
    #			[ -LockDetect	=>  ]
    #			[ -Verbose	=> boolean ]
    #			;

    my $pkg = shift ;
    my $got = BerkeleyDB::ParseParameters({
					Home		=> undef,
					Server		=> undef,
					Mode		=> 0666,
					ErrFile  	=> undef,
					ErrPrefix 	=> undef,
					Flags     	=> 0,
					Cachesize     	=> 0,
					LockDetect     	=> 0,
					Verbose		=> 0,
					Config		=> undef,
					}, @_) ;

    if (defined $got->{ErrFile}) {
	if (!isaFilehandle($got->{ErrFile})) {
	    my $handle = new IO::File ">$got->{ErrFile}"
		or croak "Cannot open file $got->{ErrFile}: $!\n" ;
	    $got->{ErrFile} = $handle ;
	}
    }

    
    my %config ;
    if (defined $got->{Config}) {
    	croak("Config parameter must be a hash reference")
            if ! ref $got->{Config} eq 'HASH' ;

	%config = %{ $got->{Config} } ;
        @BerkeleyDB::a = () ;
	my $k = "" ; my $v = "" ;
	while (($k, $v) = each %config) {
	    if ($BerkeleyDB::db_version >= 3.1 && ! $valid_config_keys{$k} ) {
	        $BerkeleyDB::Error = "illegal name-value pair: $k $v\n" ; 
                croak $BerkeleyDB::Error ;
	    }
	    push @BerkeleyDB::a, "$k\t$v" ;
	}

        $got->{"Config"} = pack("p*", @BerkeleyDB::a, undef) 
	    if @BerkeleyDB::a ;
    }

    my ($addr) = _db_appinit($pkg, $got) ;
    my $obj ;
    $obj = bless [$addr] , $pkg if $addr ;
    if ($obj && $BerkeleyDB::db_version >= 3.1 && keys %config) {
	my ($k, $v);
	while (($k, $v) = each %config) {
	    if ($k eq 'DB_DATA_DIR')
	      { $obj->set_data_dir($v) }
	    elsif ($k eq 'DB_LOG_DIR')
	      { $obj->set_lg_dir($v) }
	    elsif ($k eq 'DB_TEMP_DIR')
	      { $obj->set_tmp_dir($v) }
	    else {
	      $BerkeleyDB::Error = "illegal name-value pair: $k $v\n" ; 
              croak $BerkeleyDB::Error 
            }
	}
    }
    return $obj ;
}


sub TxnMgr
{
    my $env = shift ;
    my ($addr) = $env->_TxnMgr() ;
    my $obj ;
    $obj = bless [$addr, $env] , "BerkeleyDB::TxnMgr" if $addr ;
    return $obj ;
}

sub txn_begin
{
    my $env = shift ;
    my ($addr) = $env->_txn_begin(@_) ;
    my $obj ;
    $obj = bless [$addr, $env] , "BerkeleyDB::Txn" if $addr ;
    return $obj ;
}

sub DESTROY
{
    my $self = shift ;
    $self->_DESTROY() ;
}

package BerkeleyDB::Hash ;

use vars qw(@ISA) ;
@ISA = qw( BerkeleyDB::Common BerkeleyDB::_tiedHash ) ;
use UNIVERSAL qw( isa ) ;
use Carp ;

sub new
{
    my $self = shift ;
    my $got = BerkeleyDB::ParseParameters(
		      {
			# Generic Stuff
			Filename 	=> undef,
			Subname		=> undef,
			#Flags		=> BerkeleyDB::DB_CREATE(),
			Flags		=> 0,
			Property	=> 0,
			Mode		=> 0666,
			Cachesize 	=> 0,
			Lorder 		=> 0,
			Pagesize 	=> 0,
			Env		=> undef,
			#Tie 		=> undef,
			Txn		=> undef,

			# Hash specific
			Ffactor		=> 0,
			Nelem 		=> 0,
			Hash 		=> undef,
			DupCompare	=> undef,

			# BerkeleyDB specific
			ReadKey		=> undef,
			WriteKey	=> undef,
			ReadValue	=> undef,
			WriteValue	=> undef,
		      }, @_) ;

    croak("Env not of type BerkeleyDB::Env")
	if defined $got->{Env} and ! isa($got->{Env},'BerkeleyDB::Env');

    croak("Txn not of type BerkeleyDB::Txn")
	if defined $got->{Txn} and ! isa($got->{Txn},'BerkeleyDB::Txn');

    croak("-Tie needs a reference to a hash")
	if defined $got->{Tie} and $got->{Tie} !~ /HASH/ ;

    my ($addr) = _db_open_hash($self, $got);
    my $obj ;
    if ($addr) {
        $obj = bless [$addr] , $self ;
	push @{ $obj }, $got->{Env} if $got->{Env} ;
        $obj->Txn($got->{Txn}) if $got->{Txn} ;
    }
    return $obj ;
}

*TIEHASH = \&new ;

 
package BerkeleyDB::Btree ;

use vars qw(@ISA) ;
@ISA = qw( BerkeleyDB::Common BerkeleyDB::_tiedHash ) ;
use UNIVERSAL qw( isa ) ;
use Carp ;

sub new
{
    my $self = shift ;
    my $got = BerkeleyDB::ParseParameters(
		      {
			# Generic Stuff
			Filename 	=> undef,
			Subname		=> undef,
			#Flags		=> BerkeleyDB::DB_CREATE(),
			Flags		=> 0,
			Property	=> 0,
			Mode		=> 0666,
			Cachesize 	=> 0,
			Lorder 		=> 0,
			Pagesize 	=> 0,
			Env		=> undef,
			#Tie 		=> undef,
			Txn		=> undef,

			# Btree specific
			Minkey		=> 0,
			Compare		=> undef,
			DupCompare	=> undef,
			Prefix 		=> undef,
		      }, @_) ;

    croak("Env not of type BerkeleyDB::Env")
	if defined $got->{Env} and ! isa($got->{Env},'BerkeleyDB::Env');

    croak("Txn not of type BerkeleyDB::Txn")
	if defined $got->{Txn} and ! isa($got->{Txn},'BerkeleyDB::Txn');

    croak("-Tie needs a reference to a hash")
	if defined $got->{Tie} and $got->{Tie} !~ /HASH/ ;

    my ($addr) = _db_open_btree($self, $got);
    my $obj ;
    if ($addr) {
        $obj = bless [$addr] , $self ;
	push @{ $obj }, $got->{Env} if $got->{Env} ;
        $obj->Txn($got->{Txn}) if $got->{Txn} ;
    }
    return $obj ;
}

*BerkeleyDB::Btree::TIEHASH = \&BerkeleyDB::Btree::new ;


package BerkeleyDB::Recno ;

use vars qw(@ISA) ;
@ISA = qw( BerkeleyDB::Common BerkeleyDB::_tiedArray ) ;
use UNIVERSAL qw( isa ) ;
use Carp ;

sub new
{
    my $self = shift ;
    my $got = BerkeleyDB::ParseParameters(
		      {
			# Generic Stuff
			Filename 	=> undef,
			Subname		=> undef,
			#Flags		=> BerkeleyDB::DB_CREATE(),
			Flags		=> 0,
			Property	=> 0,
			Mode		=> 0666,
			Cachesize 	=> 0,
			Lorder 		=> 0,
			Pagesize 	=> 0,
			Env		=> undef,
			#Tie 		=> undef,
			Txn		=> undef,

			# Recno specific
			Delim		=> undef,
			Len		=> undef,
			Pad		=> undef,
			Source 		=> undef,
			ArrayBase 	=> 1, # lowest index in array
		      }, @_) ;

    croak("Env not of type BerkeleyDB::Env")
	if defined $got->{Env} and ! isa($got->{Env},'BerkeleyDB::Env');

    croak("Txn not of type BerkeleyDB::Txn")
	if defined $got->{Txn} and ! isa($got->{Txn},'BerkeleyDB::Txn');

    croak("Tie needs a reference to an array")
	if defined $got->{Tie} and $got->{Tie} !~ /ARRAY/ ;

    croak("ArrayBase can only be 0 or 1, parsed $got->{ArrayBase}")
	if $got->{ArrayBase} != 1 and $got->{ArrayBase} != 0 ;


    $got->{Fname} = $got->{Filename} if defined $got->{Filename} ;

    my ($addr) = _db_open_recno($self, $got);
    my $obj ;
    if ($addr) {
        $obj = bless [$addr] , $self ;
	push @{ $obj }, $got->{Env} if $got->{Env} ;
        $obj->Txn($got->{Txn}) if $got->{Txn} ;
    }	
    return $obj ;
}

*BerkeleyDB::Recno::TIEARRAY = \&BerkeleyDB::Recno::new ;
*BerkeleyDB::Recno::db_stat = \&BerkeleyDB::Btree::db_stat ;

package BerkeleyDB::Queue ;

use vars qw(@ISA) ;
@ISA = qw( BerkeleyDB::Common BerkeleyDB::_tiedArray ) ;
use UNIVERSAL qw( isa ) ;
use Carp ;

sub new
{
    my $self = shift ;
    my $got = BerkeleyDB::ParseParameters(
		      {
			# Generic Stuff
			Filename 	=> undef,
			Subname		=> undef,
			#Flags		=> BerkeleyDB::DB_CREATE(),
			Flags		=> 0,
			Property	=> 0,
			Mode		=> 0666,
			Cachesize 	=> 0,
			Lorder 		=> 0,
			Pagesize 	=> 0,
			Env		=> undef,
			#Tie 		=> undef,
			Txn		=> undef,

			# Queue specific
			Len		=> undef,
			Pad		=> undef,
			ArrayBase 	=> 1, # lowest index in array
			ExtentSize      => undef,
		      }, @_) ;

    croak("Env not of type BerkeleyDB::Env")
	if defined $got->{Env} and ! isa($got->{Env},'BerkeleyDB::Env');

    croak("Txn not of type BerkeleyDB::Txn")
	if defined $got->{Txn} and ! isa($got->{Txn},'BerkeleyDB::Txn');

    croak("Tie needs a reference to an array")
	if defined $got->{Tie} and $got->{Tie} !~ /ARRAY/ ;

    croak("ArrayBase can only be 0 or 1, parsed $got->{ArrayBase}")
	if $got->{ArrayBase} != 1 and $got->{ArrayBase} != 0 ;


    my ($addr) = _db_open_queue($self, $got);
    my $obj ;
    if ($addr) {
        $obj = bless [$addr] , $self ;
	push @{ $obj }, $got->{Env} if $got->{Env} ;
        $obj->Txn($got->{Txn}) if $got->{Txn} ;
    }	
    return $obj ;
}

*BerkeleyDB::Queue::TIEARRAY = \&BerkeleyDB::Queue::new ;

## package BerkeleyDB::Text ;
## 
## use vars qw(@ISA) ;
## @ISA = qw( BerkeleyDB::Common BerkeleyDB::_tiedArray ) ;
## use UNIVERSAL qw( isa ) ;
## use Carp ;
## 
## sub new
## {
##     my $self = shift ;
##     my $got = BerkeleyDB::ParseParameters(
## 		      {
## 			# Generic Stuff
## 			Filename 	=> undef,
## 			#Flags		=> BerkeleyDB::DB_CREATE(),
## 			Flags		=> 0,
## 			Property	=> 0,
## 			Mode		=> 0666,
## 			Cachesize 	=> 0,
## 			Lorder 		=> 0,
## 			Pagesize 	=> 0,
## 			Env		=> undef,
## 			#Tie 		=> undef,
## 			Txn		=> undef,
## 
## 			# Recno specific
## 			Delim		=> undef,
## 			Len		=> undef,
## 			Pad		=> undef,
## 			Btree 		=> undef,
## 		      }, @_) ;
## 
##     croak("Env not of type BerkeleyDB::Env")
## 	if defined $got->{Env} and ! isa($got->{Env},'BerkeleyDB::Env');
## 
##     croak("Txn not of type BerkeleyDB::Txn")
## 	if defined $got->{Txn} and ! isa($got->{Txn},'BerkeleyDB::Txn');
## 
##     croak("-Tie needs a reference to an array")
## 	if defined $got->{Tie} and $got->{Tie} !~ /ARRAY/ ;
## 
##     # rearange for recno
##     $got->{Source} = $got->{Filename} if defined $got->{Filename} ;
##     delete $got->{Filename} ;
##     $got->{Fname} = $got->{Btree} if defined $got->{Btree} ;
##     return BerkeleyDB::Recno::_db_open_recno($self, $got);
## }
## 
## *BerkeleyDB::Text::TIEARRAY = \&BerkeleyDB::Text::new ;
## *BerkeleyDB::Text::db_stat = \&BerkeleyDB::Btree::db_stat ;

package BerkeleyDB::Unknown ;

use vars qw(@ISA) ;
@ISA = qw( BerkeleyDB::Common BerkeleyDB::_tiedArray ) ;
use UNIVERSAL qw( isa ) ;
use Carp ;

sub new
{
    my $self = shift ;
    my $got = BerkeleyDB::ParseParameters(
		      {
			# Generic Stuff
			Filename 	=> undef,
			Subname		=> undef,
			#Flags		=> BerkeleyDB::DB_CREATE(),
			Flags		=> 0,
			Property	=> 0,
			Mode		=> 0666,
			Cachesize 	=> 0,
			Lorder 		=> 0,
			Pagesize 	=> 0,
			Env		=> undef,
			#Tie 		=> undef,
			Txn		=> undef,

		      }, @_) ;

    croak("Env not of type BerkeleyDB::Env")
	if defined $got->{Env} and ! isa($got->{Env},'BerkeleyDB::Env');

    croak("Txn not of type BerkeleyDB::Txn")
	if defined $got->{Txn} and ! isa($got->{Txn},'BerkeleyDB::Txn');

    croak("-Tie needs a reference to a hash")
	if defined $got->{Tie} and $got->{Tie} !~ /HASH/ ;

    my ($addr, $type) = _db_open_unknown($got);
    my $obj ;
    if ($addr) {
        $obj = bless [$addr], "BerkeleyDB::$type" ;
	push @{ $obj }, $got->{Env} if $got->{Env} ;
        $obj->Txn($got->{Txn}) if $got->{Txn} ;
    }	
    return $obj ;
}


package BerkeleyDB::_tiedHash ;

use Carp ;

#sub TIEHASH  
#{ 
#    my $self = shift ;
#    my $db_object = shift ;
#
#print "Tiehash REF=[$self] [" . (ref $self) . "]\n" ;
#
#    return bless { Obj => $db_object}, $self ; 
#}

sub Tie
{
    # Usage:
    #
    #   $db->Tie \%hash ;
    #

    my $self = shift ;

    #print "Tie method REF=[$self] [" . (ref $self) . "]\n" ;

    croak("usage \$x->Tie \\%hash\n") unless @_ ;
    my $ref  = shift ; 

    croak("Tie needs a reference to a hash")
	if defined $ref and $ref !~ /HASH/ ;

    #tie %{ $ref }, ref($self), $self ; 
    tie %{ $ref }, "BerkeleyDB::_tiedHash", $self ; 
    return undef ;
}

 
sub TIEHASH  
{ 
    my $self = shift ;
    my $db_object = shift ;
    #return bless $db_object, 'BerkeleyDB::Common' ; 
    return $db_object ;
}

sub STORE
{
    my $self = shift ;
    my $key  = shift ;
    my $value = shift ;

    $self->db_put($key, $value) ;
}

sub FETCH
{
    my $self = shift ;
    my $key  = shift ;
    my $value = undef ;
    $self->db_get($key, $value) ;

    return $value ;
}

sub EXISTS
{
    my $self = shift ;
    my $key  = shift ;
    my $value = undef ;
    $self->db_get($key, $value) == 0 ;
}

sub DELETE
{
    my $self = shift ;
    my $key  = shift ;
    $self->db_del($key) ;
}

sub CLEAR
{
    my $self = shift ;
    my ($key, $value) = (0, 0) ;
    my $cursor = $self->db_cursor() ;
    while ($cursor->c_get($key, $value, BerkeleyDB::DB_PREV()) == 0) 
	{ $cursor->c_del() }
    #1 while $cursor->c_del() == 0 ;
    # cursor will self-destruct
}

#sub DESTROY
#{
#    my $self = shift ;
#    print "BerkeleyDB::_tieHash::DESTROY\n" ;
#    $self->{Cursor}->c_close() if $self->{Cursor} ;
#}

package BerkeleyDB::_tiedArray ;

use Carp ;

sub Tie
{
    # Usage:
    #
    #   $db->Tie \@array ;
    #

    my $self = shift ;

    #print "Tie method REF=[$self] [" . (ref $self) . "]\n" ;

    croak("usage \$x->Tie \\%hash\n") unless @_ ;
    my $ref  = shift ; 

    croak("Tie needs a reference to an array")
	if defined $ref and $ref !~ /ARRAY/ ;

    #tie %{ $ref }, ref($self), $self ; 
    tie @{ $ref }, "BerkeleyDB::_tiedArray", $self ; 
    return undef ;
}

 
#sub TIEARRAY  
#{ 
#    my $self = shift ;
#    my $db_object = shift ;
#
#print "Tiearray REF=[$self] [" . (ref $self) . "]\n" ;
#
#    return bless { Obj => $db_object}, $self ; 
#}

sub TIEARRAY  
{ 
    my $self = shift ;
    my $db_object = shift ;
    #return bless $db_object, 'BerkeleyDB::Common' ; 
    return $db_object ;
}

sub STORE
{
    my $self = shift ;
    my $key  = shift ;
    my $value = shift ;

    $self->db_put($key, $value) ;
}

sub FETCH
{
    my $self = shift ;
    my $key  = shift ;
    my $value = undef ;
    $self->db_get($key, $value) ;

    return $value ;
}

*CLEAR =    \&BerkeleyDB::_tiedHash::CLEAR ;
*FIRSTKEY = \&BerkeleyDB::_tiedHash::FIRSTKEY ;
*NEXTKEY =  \&BerkeleyDB::_tiedHash::NEXTKEY ;

sub EXTEND {} # don't do anything with EXTEND


sub SHIFT
{
    my $self = shift;
    my ($key, $value) = (0, 0) ;
    my $cursor = $self->db_cursor() ;
    return undef if $cursor->c_get($key, $value, BerkeleyDB::DB_FIRST()) != 0 ;
    return undef if $cursor->c_del() != 0 ;

    return $value ;
}


sub UNSHIFT
{
    my $self = shift;
    croak "unshift is unsupported with Queue databases"
        if $self->type == BerkeleyDB::DB_QUEUE() ;
    if (@_)
    {
        my ($key, $value) = (0, 0) ;
        my $cursor = $self->db_cursor() ;
        if ($cursor->c_get($key, $value, BerkeleyDB::DB_FIRST()) == 0) 
        {
            foreach $value (reverse @_)
            {
	        $key = 0 ;
	        $cursor->c_put($key, $value, BerkeleyDB::DB_BEFORE()) ;
            }
        }
    }
}

sub PUSH
{
    my $self = shift;
    if (@_)
    {
        my ($key, $value) = (0, 0) ;
        my $cursor = $self->db_cursor() ;
        if ($cursor->c_get($key, $value, BerkeleyDB::DB_LAST()) == 0)
	{
            foreach $value (@_)
	    {
	        ++ $key ;
	        $self->db_put($key, $value) ;
	    }
	}

# can use this when DB_APPEND is fixed.
#        foreach $value (@_)
#        {
#	    my $status = $cursor->c_put($key, $value, BerkeleyDB::DB_AFTER()) ;
#print "[$status]\n" ;
#        }
    }
}

sub POP
{
    my $self = shift;
    my ($key, $value) = (0, 0) ;
    my $cursor = $self->db_cursor() ;
    return undef if $cursor->c_get($key, $value, BerkeleyDB::DB_LAST()) != 0 ;
    return undef if $cursor->c_del() != 0 ;

    return $value ;
}

sub SPLICE
{
    my $self = shift;
    croak "SPLICE is not implemented yet" ;
}

*shift = \&SHIFT ;
*unshift = \&UNSHIFT ;
*push = \&PUSH ;
*pop = \&POP ;
*clear = \&CLEAR ;
*length = \&FETCHSIZE ;

sub STORESIZE
{
    croak "STORESIZE is not implemented yet" ;
#print "STORESIZE @_\n" ;
#    my $self = shift;
#    my $length = shift ;
#    my $current_length = $self->FETCHSIZE() ;
#print "length is $current_length\n";
#
#    if ($length < $current_length) {
#print "Make smaller $length < $current_length\n" ;
#        my $key ;
#        for ($key = $current_length - 1 ; $key >= $length ; -- $key)
#          { $self->db_del($key) }
#    }
#    elsif ($length > $current_length) {
#print "Make larger $length > $current_length\n" ;
#        $self->db_put($length-1, "") ;
#    }
#    else { print "stay the same\n" }

}



#sub DESTROY
#{
#    my $self = shift ;
#    print "BerkeleyDB::_tieArray::DESTROY\n" ;
#}


package BerkeleyDB::Common ;


use Carp ;

sub DESTROY
{
    my $self = shift ;
    $self->_DESTROY() ;
}

sub Txn
{
    my $self = shift ;
    my $txn  = shift ;
    #print "BerkeleyDB::Common::Txn db [$self] txn [$txn]\n" ;
    if ($txn) {
        $self->_Txn($txn) ;
        push @{ $txn }, $self ;
    }
    else {
        $self->_Txn() ;
    }
    #print "end BerkeleyDB::Common::Txn \n";
}


sub get_dup
{
    croak "Usage: \$db->get_dup(key [,flag])\n"
        unless @_ == 2 or @_ == 3 ;
 
    my $db        = shift ;
    my $key       = shift ;
    my $flag	  = shift ;
    my $value 	  = 0 ;
    my $origkey   = $key ;
    my $wantarray = wantarray ;
    my %values	  = () ;
    my @values    = () ;
    my $counter   = 0 ;
    my $status    = 0 ;
    my $cursor    = $db->db_cursor() ;
 
    # iterate through the database until either EOF ($status == 0)
    # or a different key is encountered ($key ne $origkey).
    for ($status = $cursor->c_get($key, $value, BerkeleyDB::DB_SET()) ;
	 $status == 0 and $key eq $origkey ;
         $status = $cursor->c_get($key, $value, BerkeleyDB::DB_NEXT()) ) {
        # save the value or count number of matches
        if ($wantarray) {
	    if ($flag)
                { ++ $values{$value} }
	    else
                { push (@values, $value) }
	}
        else
            { ++ $counter }
     
    }
 
    return ($wantarray ? ($flag ? %values : @values) : $counter) ;
}

sub db_cursor
{
    my $db = shift ;
    my ($addr) = $db->_db_cursor(@_) ;
    my $obj ;
    $obj = bless [$addr, $db] , "BerkeleyDB::Cursor" if $addr ;
    return $obj ;
}

sub db_join
{
    croak 'Usage: $db->BerkeleyDB::Common::db_join([cursors], flags=0)'
	if @_ < 2 || @_ > 3 ;
    my $db = shift ;
    my ($addr) = $db->_db_join(@_) ;
    my $obj ;
    $obj = bless [$addr, $db] , "BerkeleyDB::Cursor" if $addr ;
    return $obj ;
}

package BerkeleyDB::Cursor ;

sub c_close
{
    my $cursor = shift ;
    $cursor->[1] = "" ;
    return $cursor->_c_close() ;
}

sub c_dup
{
    my $cursor = shift ;
    my ($addr) = $cursor->_c_dup(@_) ;
    my $obj ;
    $obj = bless [$addr, $cursor->[1]] , "BerkeleyDB::Cursor" if $addr ;
    return $obj ;
}

sub DESTROY
{
    my $self = shift ;
    $self->_DESTROY() ;
}

package BerkeleyDB::TxnMgr ;

sub DESTROY
{
    my $self = shift ;
    $self->_DESTROY() ;
}

sub txn_begin
{
    my $txnmgr = shift ;
    my ($addr) = $txnmgr->_txn_begin(@_) ;
    my $obj ;
    $obj = bless [$addr, $txnmgr] , "BerkeleyDB::Txn" if $addr ;
    return $obj ;
}

package BerkeleyDB::Txn ;

sub Txn
{
    my $self = shift ;
    my $db ;
    # keep a reference to each db in the txn object
    foreach $db (@_) {
        $db->_Txn($self) ;
	push @{ $self}, $db ;
    }
}

sub txn_commit
{
    my $self = shift ;
    $self->disassociate() ;
    my $status = $self->_txn_commit() ;
    return $status ;
}

sub txn_abort
{
    my $self = shift ;
    $self->disassociate() ;
    my $status = $self->_txn_abort() ;
    return $status ;
}

sub disassociate
{
    my $self = shift ;
    my $db ;
    while ( @{ $self } > 2) {
        $db = pop @{ $self } ;
        $db->Txn() ;
    }
    #print "end disassociate\n" ;
}


sub DESTROY
{
    my $self = shift ;

    $self->disassociate() ;
    # first close the close the transaction
    $self->_DESTROY() ;
}

package BerkeleyDB::Term ;

END
{
    close_everything() ;
}


package BerkeleyDB ;



# Autoload methods go after =cut, and are processed by the autosplit program.

1;
__END__


