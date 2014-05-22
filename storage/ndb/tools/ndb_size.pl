#!/usr/bin/perl -w

# Copyright (c) 2005, 2013, Oracle and/or its affiliates. All rights reserved.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; version 2 of the License.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

use strict;

use DBI;
use POSIX;
use Getopt::Long;

# MySQL Cluster size estimator
# ----------------------------
#
# The purpose of this tool is to work out storage requirements
# from an existing MySQL database.
#
# This involves connecting to a mysql server and throwing a bunch
# of queries at it.
#
# We currently estimate sizes for: 4.1, 5.0 and 5.1 to various amounts
# of accurracy.
#
# There is no warranty.
#
# BUGS
# ----
# - DECIMAL is 0 byte storage. A bit too efficient.
# - some float stores come out weird (when there's a comma e.g. 'float(4,1)')
# - no disk data values
# - computes the storage requirements of views (and probably MERGE)
# - ignores character sets?

package MySQL::NDB::Size::Parameter;

use Class::MethodMaker [
			scalar => 'name',
			scalar => 'default',
			scalar => 'mem_per_item',
			scalar => [{ -default => '' },'unit'],
			hash   => [ qw ( value
					 ver_mem_per_item
					 mem_total ) ],
			new    => [ -hash => 'new' ],
			];

1;

package MySQL::NDB::Size::Report;

use Class::MethodMaker [
			scalar => [ qw( database
					dsn ) ],
			array  => 'versions',
			hash   => [qw( tables
				       parameters
				       supporting_tables) ],
			new    => [ -hash => 'new' ],
			];
1;

package MySQL::NDB::Size::Column;

use Class::MethodMaker [
			new    => [ -hash => 'new' ],
			scalar => [ qw( name
					type
					is_varsize
					size
					Key) ],
			hash   => 'dm_overhead',
			scalar => [{ -default => 4 },'align'],
			scalar => [{ -default => 0 },'null_bits'],
			];

# null_bits:
#   0 if not nullable, 1 if nullable
#   + additional if bitfield as these are stored in the null bits
#  if is_varsize, null_bits are in varsize part.

# dm is default DataMemory value. Automatically 4byte aligned
# ver_dm is DataMemory value for specific versions.
#   an entry in ver_dm OVERRIDES the dm value.
# e.g. if way column stored changed in new version.
#
# if is_varsize, dm/ver_dm is in varsized part.

sub ver_dm_exists
{
    my ($self,$ver)= @_;
    return exists($self->{ver_dm}{$ver});
}

use Data::Dumper;
sub ver_dm
{
    my ($self,$ver,$val)= @_;
    if(@_ > 2)
    {
	$self->{ver_dm}{$ver}=
	    $self->align * POSIX::floor(($val+$self->align-1)/$self->align);
    }
    return $self->{ver_dm}{$ver};
}

sub dm
{
    my ($self,$val)= @_;
    if(@_ > 1)
    {
	$self->{dm}=
	    $self->align * POSIX::floor(($val+$self->align-1)/$self->align)
    }
    return $self->{dm};
}

package MySQL::NDB::Size::Index;

use Class::MethodMaker [
			new    => [ -hash => 'new' ],
			hash   => [ qw( ver_dm
					ver_im ) ],
			scalar => [ qw( name
					type
					comment
					columns
					unique
					dm
					im) ],
			scalar => [ { -default=> 4 },'align'],
			scalar => [ { -default=> 0 },'is_supporting_table' ],
			];

package MySQL::NDB::Size::Table;

# The following are computed by compute_row_size:
#  row_dm_size    DataMemory Size per row
#  row_vdm_size   Varsized DataMemory Size per row
#  row_ddm_size   Disk Data size per row (on disk size)
#
# These are hashes of versions. If an entry in (dm|vdm|ddm)_versions exists,
# then this parameter is calculated.
#
# Specific per-row overhead is in row_(dm|vdm|ddm)_overhead.
# e.g. if there is a varsized column, we have a vdm overhead for the
# varsized part of the row, otherwise vdm_size==0

# Any supporting tables - e.g. BLOBs have their name in supporting_tables
# These tables should then be looked up in the main report object.
# The main report object also has a supporting_tables hash used for
# excluding these from the main list of tables.
use POSIX;
use Class::MethodMaker [
			new    => [ -hash => 'new' ],
			array  => [ qw( supporting_tables
					dm_versions
					vdm_versions
					ddm_versions ) ],
			scalar => [ qw( name
					rows
                                        schema
                                        real_table_name) ],
			hash   => [ qw( columns
					indexes
					indexed_columns
					row_im_size
					row_dm_size
					row_vdm_size
					row_dm_overhead
					row_vdm_overhead
					row_ddm_overhead) ],
			scalar => [ { -default=> 8192 },'im_pagesize'],
			scalar => [ { -default=> 0 },'im_page_overhead'],
			scalar => [ { -default=> 32768 },'dm_pagesize' ],
			scalar => [ { -default=> 128 },'dm_page_overhead' ],
			scalar => [ { -default=> 32768 },'vdm_pagesize' ],
			scalar => [ { -default=> 128 },'vdm_page_overhead' ],
			hash   => [ # these are computed
				    qw(
					dm_null_bytes
					vdm_null_bytes
					dm_needed
					vdm_needed
					im_needed
				       im_rows_per_page
				       dm_rows_per_page
				       vdm_rows_per_page) ],
			scalar => [ { -default=> 4 },'align'],
			];

sub table_name
{
    my ($self) = @_;
    if ($self->real_table_name) {
	return $self->real_table_name;
    }else {
	return $self->name;
    }
}

sub compute_row_size
{
    my ($self, $releases) = @_;

    my %row_dm_size;
    my %row_vdm_size;
    my %row_im_size;
    my %dm_null_bits;
    my %vdm_null_bits;
    my $no_varsize= 0;

    foreach my $c (keys %{$self->columns})
    {
	if($self->columns->{$c}->is_varsize)
	{
	    $no_varsize++;
	    foreach my $ver ($self->vdm_versions)
	    {
		if($self->columns->{$c}->ver_dm_exists($ver))
		{
		    $row_vdm_size{$ver}+= $self->columns->{$c}->ver_dm($ver);
		    $vdm_null_bits{$ver}+= $self->columns->{$c}->null_bits();
		}
		else
		{
		    $row_vdm_size{$ver}+= $self->columns->{$c}->dm;
		    $vdm_null_bits{$ver}+= $self->columns->{$c}->null_bits();
		}
	    }
	}
	foreach my $ver ($self->dm_versions)
	{
	    if($self->columns->{$c}->ver_dm_exists($ver))
	    {
		next if $self->columns->{$c}->is_varsize;
		$row_dm_size{$ver}+= $self->columns->{$c}->ver_dm($ver);
		$dm_null_bits{$ver}+= $self->columns->{$c}->null_bits();
	    }
	    else
	    {
		$row_dm_size{$ver}+= $self->columns->{$c}->dm||0;
		$dm_null_bits{$ver}+= $self->columns->{$c}->null_bits()||0;
	    }
	}
    }

    foreach ($self->row_dm_overhead_keys())
    {
	$row_dm_size{$_}+= $self->row_dm_overhead->{$_}
	if exists($row_dm_size{$_});
    }


    foreach ($self->row_vdm_overhead_keys())
    {
	$row_vdm_size{$_}+= $self->row_vdm_overhead->{$_}
	if exists($row_vdm_size{$_});
    }


    # now we compute size of indexes for dm
    foreach my $i (keys %{$self->indexes})
    {
	foreach my $ver ($self->dm_versions)
	{
	    $row_dm_size{$ver}+= $self->indexes->{$i}->dm() || 0;
	}
    }

    # now we compute size of indexes for im
    while(my ($iname, $i) = $self->indexes_each())
    {
	foreach my $ver ($self->dm_versions)
	{
	    if($i->ver_im_exists($ver))
	    {
		$row_im_size{$ver}+= $i->ver_im->{$ver};
	    }
	    else
	    {
		$row_im_size{$ver}+= $i->im() || 0;
	    }
	}
    }

    # 32-bit align the null part
    foreach my $k (keys %dm_null_bits)
    {
	$dm_null_bits{$k}=
	    $self->align * POSIX::floor((ceil($dm_null_bits{$k}/8)+$self->align-1)
					/$self->align);
    }

    foreach my $k (keys %vdm_null_bits)
    {
	$vdm_null_bits{$k}=
	    $self->align * POSIX::floor((ceil($vdm_null_bits{$k}/8)+$self->align-1)
					/$self->align);
    }

    # Finally set things
    $self->dm_null_bytes(%dm_null_bits);
    $self->vdm_null_bytes(%vdm_null_bits);

    # add null bytes to dm/vdm size
    foreach my $k (keys %row_dm_size)
    {
	$row_dm_size{$k}+=$dm_null_bits{$k}||0;
    }

    foreach my $k (keys %row_vdm_size)
    {
	$row_vdm_size{$k}+=$vdm_null_bits{$k}||0;
    }

    $self->row_dm_size(%row_dm_size);
    $self->row_vdm_size(%row_vdm_size);
    $self->row_im_size(%row_im_size);
}

sub compute_estimate
{
    my ($self) = @_;

    foreach my $ver (@{$self->dm_versions})
    {
	$self->dm_rows_per_page_set($ver =>
	    floor(
		  ($self->dm_pagesize - $self->dm_page_overhead)
		  /
		  $self->row_dm_size->{$ver}
		  )
				    );
    }

    foreach my $ver (@{$self->vdm_versions})
    {
	next if ! $self->row_vdm_size_exists($ver);
	$self->vdm_rows_per_page_set($ver =>
	    floor(
		  ($self->vdm_pagesize - $self->vdm_page_overhead)
		  /
		  $self->row_vdm_size->{$ver}
		  )
				     );
    }

    $self->im_page_overhead(0) if !$self->im_page_overhead();
    foreach my $ver (@{$self->dm_versions})
    {
	$self->im_rows_per_page_set($ver =>
	    floor(
		  ($self->im_pagesize - $self->im_page_overhead)
		  /
		  $self->row_im_size->{$ver}
		  )
				    );
    }

    $self->dm_needed_set($_ => $self->dm_pagesize()
	*
	POSIX::ceil(
		    $self->rows
		    /
		    ($self->dm_rows_per_page->{$_})
		    )
		     )
	foreach $self->dm_versions;

    $self->vdm_needed_set($_ => (!$self->vdm_rows_per_page->{$_})? 0 :
			  $self->vdm_pagesize()
			  *
			  POSIX::ceil(
				      $self->rows
				      /
				      ($self->vdm_rows_per_page->{$_})
				      )
			  )
	foreach $self->vdm_versions;

    $self->im_needed_set($_ => $self->im_pagesize()
	*
	POSIX::ceil(
		    $self->rows
		    /
		    ($self->im_rows_per_page->{$_})
		    )
		     )
	foreach $self->dm_versions;
}

package main;

my ($dbh,
    $database,
    $socket,
    $hostname,
    $user,
    $password);

my ($help,
    $savequeries,
    $loadqueries,
    $debug,
    $format,
    $excludetables,
    $excludedbs);

GetOptions('database|d=s'=>\$database,
	   'hostname=s'=>\$hostname,
	   'socket=s'=>\$socket,
	   'user|u=s'=>\$user,
	   'password|p=s'=>\$password,
	   'savequeries|s=s'=>\$savequeries,
	   'loadqueries|l=s'=>\$loadqueries,
	   'excludetables=s'=>\$excludetables,
	   'excludedbs=s'=>\$excludedbs,
	   'help|usage|h!'=>\$help,
	   'debug'=>\$debug,
	   'format|f=s'=>\$format,
	   );

my $report= new MySQL::NDB::Size::Report;

if($help)
{
    print STDERR "Usage:\n";
    print STDERR "\tndb_size.pl --database=<db name>|ALL [--hostname=<host>] "
	."[--socket=<socket>] "
	."[--user=<user>] [--password=<password>] [--help|-h] [--format=(html|text)] [--loadqueries=<file>] [--savequeries=<file>]\n\n";
    print STDERR "\t--database=<db name> ALL may be specified to examine all "
	."databases\n";
    print STDERR "\t--hostname=<host>:<port> can be used to designate a "
	."specific port\n";
    print STDERR "\t--hostname defaults to localhost\n";
    print STDERR "\t--user and --password default to empty string\n";
    print STDERR "\t--format=(html|text) Output format\n";
    print STDERR "\t--excludetables Comma separated list of table names to skip\n";
    print STDERR "\t--excludedbs Comma separated list of database names to skip\n";
    print STDERR "\t--savequeries=<file> saves all queries to the DB into <file>\n";
    print STDERR "\t--loadqueries=<file> loads query results from <file>. Doesn't connect to DB.\n";
    exit(1);
}


$hostname= 'localhost' unless $hostname;

my %queries; # used for loadqueries/savequeries

if(!$loadqueries)
{
    my ($host,$port) = split(/:/, $hostname);
    my $dsn = "DBI:mysql:host=$host";
    $dsn.= ";port=$port" if ($port);
    $dsn.= ";mysql_socket=$socket" if ($socket);
    $dbh= DBI->connect($dsn, $user, $password) or exit(1);
    $report->dsn($dsn);
}

my @dbs;
if ($database && !($database =~  /^ALL$/i))
{
    @dbs = split(',', $database);
}
else
{
    # Do all databases
    @dbs = map { $_->[0] } @{ $dbh->selectall_arrayref("show databases") };
}

my %withdb = map {$_ => 1} @dbs;
foreach (split ",", $excludedbs || '')
{
    delete $withdb{$_};
}
delete $withdb{'mysql'};
delete $withdb{'INFORMATION_SCHEMA'};
delete $withdb{'information_schema'};

my $dblist = join (',', map { $dbh->quote($_) } keys %withdb );

$excludetables = join (',', map { $dbh->quote($_) } split ',', $excludetables )
    if $excludetables;

if(!$loadqueries)
{
  if (scalar(keys %withdb)>1)
  {
    $report->database("databases: $dblist");
  }
  else
  {
    $report->database("database: $dblist");
  }
}
else
{
    open Q,"< $loadqueries";
    my @q= <Q>;
    my $VAR1;
    my $e= eval join("",@q) or die $@;
    %queries= %$e;
    close Q;
    $report->database("file:$loadqueries");
}

$report->versions('4.1','5.0','5.1');

my $tables;

if($loadqueries)
{
    $tables= $queries{"show tables"};
}
else
{
    my $sql= "select t.TABLE_NAME,t.TABLE_SCHEMA " .
	" from information_schema.TABLES t " .
	" where t.TABLE_SCHEMA in ( $dblist ) ";

    $sql.="   and t.TABLE_NAME not in " .
	" ( $excludetables )"
	if ($excludetables);

    $tables= $dbh->selectall_arrayref($sql);

    if (!$tables) {
	print "WARNING: problem selecing from INFORMATION SCHEMA ($sql)\n";
	if ($#dbs>0) {
	    print "\t attempting to fallback to show tables from $database";
	    $tables= $dbh->selectall_arrayref("show tables from $database\n");
	} else {
	    print "All Databases not supported in 4.1. Please specify --database=\n";
	}
    }
    $queries{"show tables"}= $tables;
}

sub do_table {
    my $t= shift;
    my $info= shift;
    my %indexes= %{$_[0]};
    my @count= @{$_[1]};

    $t->dm_versions($report->versions);
    $t->vdm_versions('5.1');
    $t->ddm_versions('5.1');

    foreach my $colname (keys %$info)
    {
	my $col= new MySQL::NDB::Size::Column(name => $colname);
	my ($type, $size);

	$col->Key($$info{$colname}{Key})
	    if(defined($$info{$colname}{Key}) &&$$info{$colname}{Key} ne '');

	$col->null_bits(defined($$info{$colname}{Null})
			&& $$info{$colname}{Null} eq 'YES');

	if(defined($$info{$colname}{Type})
	   && $$info{$colname}{Type} =~ /^(.*?)\((.+)\)/)
	{
	    $type= $1;
	    $size= $2;
	}
	elsif(exists($$info{$colname}{type}))
	{
	    # we have a Column object..
	    $type= $$info{$colname}{type};
	    $size= $$info{$colname}{size};
	}
	else
	{
	    $type= $$info{$colname}{Type};
	}
	$col->type($type);
	$col->size($size);

	if($type =~ /tinyint/)
	{$col->dm(1)}
	elsif($type =~ /smallint/)
	{$col->dm(2)}
	elsif($type =~ /mediumint/)
	{$col->dm(3)}
	elsif($type =~ /bigint/)
	{$col->dm(8)}
	elsif($type =~ /int/)
	{$col->dm(4)}
	elsif($type =~ /float/)
	{
	    my @sz= split ',', $size;
	    $size= $sz[0]+$sz[1];
	    if(!defined($size) || $size<=24)
	    {$col->dm(4)}
	    else
	    {$col->dm(8)}
	}
	elsif($type =~ /double/ || $type =~ /real/)
	{$col->dm(8)}
	elsif($type =~ /bit/)
	{
	    # bitfields stored in null bits
	    $col->null_bits($size+($col->null_bits()||0));
	}
	elsif($type =~ /datetime/)
	{$col->dm(8)}
	elsif($type =~ /timestamp/)
	{$col->dm(4)}
	elsif($type =~ /date/ || $type =~ /time/)
	{$col->dm(3)}
	elsif($type =~ /year/)
	{$col->dm(1)}
	elsif($type =~ /enum/ || $type =~ /set/)
	{
	    # I *think* this is correct..
	    my @items= split ',',$size;
	    $col->dm(ceil((scalar @items)/256));
	}
	elsif($type =~ /varchar/ || $type =~ /varbinary/)
	{
	    my $fixed=$size+ceil($size/256);
	    $col->dm_overhead_set('length' => ceil($size/256));
	    $col->dm($fixed);
	    if(!$col->Key()) # currently keys must be non varsized
	    {
		my $sql= sprintf("select avg(length(`%s`)) " .
				 " from `%s`.`%s` " ,
				 $colname, $t->schema(), $t->table_name());

		my @dynamic;
		if($loadqueries)
		{
		    @dynamic= @{$queries{$sql}};
		}
		else
		{
		    @dynamic= $dbh->selectrow_array($sql);
		    $queries{$sql}= \@dynamic;
		}
		$dynamic[0]=0 if ! defined($dynamic[0]) || !@dynamic;
		$dynamic[0]+=ceil($size/256); # size bit
		$col->is_varsize(1);
		$col->ver_dm('5.1',ceil($dynamic[0]));
	    }
	}
	elsif($type =~ /binary/ || $type =~ /char/)
	{$col->dm($size)}
	elsif($type =~ /text/ || $type =~ /blob/)
	{
	    $col->dm_overhead_set('length' => 8);
	    $col->dm(8+256);

	    my $blobhunk= 2000;
	    $blobhunk= 8000 if $type=~ /longblob/;
	    $blobhunk= 4000 if $type=~ /mediumblob/;

	    my $sql= sprintf("select SUM(CEILING(length(`%s`)/%s)) " .
			     " from `%s`.`%s`" ,
			     $colname, $blobhunk,
			     $t->schema(), $t->table_name() );

	    my @blobsize;
	    if($loadqueries)
	    {
		@blobsize= @{$queries{$sql}};
	    }
	    else
	    {
		@blobsize= $dbh->selectrow_array($sql);
		$queries{$sql}= \@blobsize;
	    }
	    $blobsize[0]=0 if !defined($blobsize[0]);

	    # Is a supporting table, add it to the lists:
	    $report->supporting_tables_set($t->schema().".".$t->name()."\$BLOB_$colname" => 1);
	    $t->supporting_tables_push($t->schema().".".$t->name()."\$BLOB_$colname");

	    my $st= new MySQL::NDB::Size::Table(name =>
						$t->name()."\$BLOB_$colname",
						schema => $t->schema(),
						rows => $blobsize[0],
						row_dm_overhead =>
						{ '4.1' => 12,
						  '5.0' => 12,
						  '5.1' => 16,
					      },
						row_vdm_overhead =>
						{ '5.1' => 8 },
						row_ddm_overhead =>
						{ '5.1' => 8 },
						);



	    do_table($st,
		     {'PK'=>{Type=>'int'},
		      'DIST'=>{Type=>'int'},
		      'PART'=>{Type=>'int'},
		      'DATA'=>{Type=>"binary($blobhunk)"}
		  },
		     {'PRIMARY' => {
                         'unique' => 1,
                         'comment' => '',
                         'columns' => [
                                        'PK',
				        'DIST',
				        'PART',
                                      ],
			 'type' => 'HASH'
				    }
		  },
		     \@blobsize);
	}

	$col->type($type);
	$col->size($size);
	$t->columns_set( $colname => $col );
    }
    #print "setting tables: ",$t->schema(), $t->table_name(), $t->name, $t->real_table_name || "" , "\n";
    # Use $t->name here instead of $t->table_name() to avoid namespace conflicts
    $report->tables_set( $t->schema().".".$t->name() => $t );

    # And now... the IndexMemory usage.
    #
    # Firstly, we assemble some information about the indexes.
    # We use SHOW INDEX instead of using INFORMATION_SCHEMA so
    # we can still connect to pre-5.0 mysqlds.

    if(!defined($indexes{PRIMARY})) {
	my $i= new MySQL::NDB::Size::Index(
				    name    => 'PRIMARY',
				    unique  => 1,
				    comment =>'Hidden pkey created by NDB',
				    type    =>'BTREE',
				    columns => ['HIDDEN_NDB_PKEY'],
					   );

	$i->im(16);
	$i->dm(16);
	$i->ver_im('4.1',25+8);

	$t->indexes_set('PRIMARY' => $i);
	$t->indexed_columns_set('HIDDEN_NDB_PKEY' => 1);

	$t->columns_set('HIDDEN_NDB_PKEY' =>
			new MySQL::NDB::Size::Column(
						     name => 'HIDDEN_NDB_PKEY',
						     type => 'bigint',
						     dm   => 8,
						     Key  => 'PRI'));
    }

    my @indexes;

    # We model the PRIMARY first as needed for secondary uniq indexes
    if(defined($indexes{'PRIMARY'}))
    {
	my $index= 'PRIMARY';
	my $i= new MySQL::NDB::Size::Index(
				name    => $index,
				unique  => $indexes{$index}{unique},
				comment => $indexes{$index}{comment},
				type    => $indexes{$index}{type},
				columns => [@{$indexes{$index}{columns}}],
					   );
	my $im41= 25; # keep old estimate for 4.1
	$im41+= $t->columns->{$_}->dm foreach @{$indexes{$index}{columns}};
	$i->im(16); # estimate from Johan
	$i->dm(16) if $indexes{$index}{unique}; # estimate from Johan
	$i->ver_im('4.1',$im41);

	$t->indexes_set($index => $i);
	$t->indexed_columns_set($_ => 1)
	    foreach @{$indexes{$index}{columns}};
    }

    foreach my $index (keys %indexes) {
	next if $index eq 'PRIMARY';

	if(!$indexes{$index}{unique})
	{
	    my $i= new MySQL::NDB::Size::Index(
				name    => $index,
				unique  => $indexes{$index}{unique},
				comment => $indexes{$index}{comment},
				type    => $indexes{$index}{type},
				columns => [@{$indexes{$index}{columns}}],
					   );
	    $i->dm(16);
	    $t->indexes_set($index => $i);
	    $t->indexed_columns_set($_ => 1)
		foreach @{$indexes{$index}{columns}};
	}
	else
	{
	    my $i= new MySQL::NDB::Size::Index(
				name    => $index,
				unique  => $indexes{$index}{unique},
				comment => $indexes{$index}{comment},
				type    => $indexes{$index}{type},
				columns => [@{$indexes{$index}{columns}},
				    @{$t->indexes->{'PRIMARY'}->columns()}],
					   );

	    $i->is_supporting_table(1);
	    $t->indexes_set($index => $i);

	    my %idxcols;
	    foreach(@{$i->columns()})
	    {
		$idxcols{$_} = $t->columns->{$_}
	    }
	    # Is a supporting table, add it to the lists:
	    my $idxname= $t->name().'_'.join('_',@{$indexes{$index}{columns}}).
		"\$unique";
	    $report->supporting_tables_set($t->schema().".".$idxname => 1);
	    $t->supporting_tables_push($t->schema().".".$idxname);

	    $t->indexed_columns_set($_ => 1)
		foreach @{$indexes{$index}{columns}};

	    my $st= new MySQL::NDB::Size::Table(name => $idxname,
						real_table_name => $t->table_name(),
						rows => $count[0],
						schema => $t->schema(),
						row_dm_overhead =>
						{ '4.1' => 12,
						  '5.0' => 12,
						  '5.1' => 16+4,
					      },
						row_vdm_overhead =>
						{ '5.1' => 8 },
						row_ddm_overhead =>
						{ '5.1' => 8 },
						);
	    do_table($st,
		     \%idxcols,
		     {
			 'PRIMARY' => {
			     'unique' => 0,#$indexes{$index}{unique},
			     'columns' => [@{$indexes{$index}{columns}}],
			     'type' => 'BTREE',
			 }
		     },
		     \@count);
	}
    }

    $t->compute_row_size($report->versions);

} # do_table

foreach(@{$tables})
{
    my $table= @{$_}[0];
    my $schema = @{$_}[1] || $database;
    my $info;
    {
	my $sql= 'describe `'.$schema.'`.`'.$table.'`';
	if($loadqueries)
	{
	    $info= $queries{$sql};
	}
	else
	{
	    $info= $dbh->selectall_hashref($sql,"Field");
	    $queries{$sql}= $info;
	}
    }
    my @count;
    {
	my $sql= 'select count(*) from `'.$schema.'`.`'.$table.'`';
	if($loadqueries)
	{
	    @count= @{$queries{$sql}};
	}
	else
	{
	    @count= $dbh->selectrow_array($sql);
	    $queries{$sql}= \@count;
	}
    }

    my %indexes;
    {
	my @show_indexes;
	{
	    my $sql= "show index from `".$schema.'`.`'.$table.'`';
	    if($loadqueries)
	    {
		@show_indexes= @{$queries{$sql}};
	    }
	    else
	    {
		my $sth= $dbh->prepare($sql);
		$sth->execute;
		while(my $i = $sth->fetchrow_hashref)
		{
		    push @show_indexes, $i;
		}
		$queries{$sql}= \@show_indexes;
	    }
	}
	foreach my $i(@show_indexes)
	{
	    $indexes{$i->{Key_name}}= {
		type=>$i->{Index_type},
		unique=>$i->{Non_unique},
		comment=>$i->{Comment},
	    } if !defined($indexes{$i->{Key_name}});

	    $indexes{$i->{Key_name}}{columns}[$i->{Seq_in_index}-1]=
		$i->{Column_name};
	}
    }
    my $t= new MySQL::NDB::Size::Table(name => $table,
				       schema => $schema,
				       rows => $count[0],
				       row_dm_overhead =>
				        { '4.1' => 12,
					  '5.0' => 12,
					  '5.1' => 16,
					  },
				       row_vdm_overhead => { '5.1' => 8 },
				       row_ddm_overhead => { '5.1' => 8 },
				       );


    do_table($t, $info, \%indexes, \@count);
}

# compute table estimates
while(my ($tname,$t)= $report->tables_each())
{
    $t->compute_estimate();
}

# Now parameters....

$report->parameters_set('NoOfTables' =>
			new MySQL::NDB::Size::Parameter(name=>'NoOfTables',
							mem_per_item=>20,
							default=>128)
			);

$report->parameters->{'NoOfTables'}->value_set($_ => scalar @{$report->tables_keys()})
    foreach $report->versions;

$report->parameters_set('NoOfAttributes' =>
			new MySQL::NDB::Size::Parameter(name=>'NoOfAttributes',
							mem_per_item=>0.2,
							default=>1000)
			);

{
    my $attr= 0;
    while(my ($tname,$t)= $report->tables_each())
    {
	$attr+= scalar @{$t->columns_keys()};
    }
    $report->parameters->{'NoOfAttributes'}->value_set($_ => $attr)
	foreach $report->versions;
}


$report->parameters_set('NoOfOrderedIndexes' =>
			new MySQL::NDB::Size::Parameter(name=>'NoOfOrderedIndexes',
							mem_per_item=>10,
							default=>128)
			);
{
    my $attr= 0;
    while(my ($tname,$t)= $report->tables_each())
    {
	next if $report->supporting_tables_exists($tname);
	$attr+= scalar @{$t->indexes_keys()};
    }
    $report->parameters->{'NoOfOrderedIndexes'}->value_set($_ => $attr)
	foreach $report->versions;
}

$report->parameters_set('NoOfUniqueHashIndexes' =>
			new MySQL::NDB::Size::Parameter(name=>'NoOfUniqueHashIndexes',
							mem_per_item=>15,
							default=>64)
			);
{
    my $attr= 0;
    while(my ($tname,$t)= $report->tables_each())
    {
	next if not $tname =~ /\$unique$/;
	$attr++;
    }
    $report->parameters->{'NoOfUniqueHashIndexes'}->value_set($_ => $attr)
	foreach $report->versions;
}

# Size of trigger is not documented
$report->parameters_set('NoOfTriggers' =>
			new MySQL::NDB::Size::Parameter(name=>'NoOfTriggers',
							mem_per_item=>0,
							default=>768)
			);

{
    $report->parameters->{'NoOfTriggers'}->value_set(
	   $_ => (
		  (3*
		   $report->parameters->{'NoOfUniqueHashIndexes'}->value->{$_})
		  +
		  $report->parameters->{'NoOfOrderedIndexes'}->value->{$_}
		  +
		  (4* # for backups (3) and replication (1??)
		   $report->parameters->{'NoOfTables'}->value->{$_})

		  )
						     )
	foreach $report->versions;
}

# DataMemory is in bytes...
$report->parameters_set('DataMemory' =>
			new MySQL::NDB::Size::Parameter(name=>'DataMemory',
							mem_per_item=>1024,
							unit=>'KB',
							default=>80*1024)
			);
$report->parameters_set('IndexMemory' =>
			new MySQL::NDB::Size::Parameter(name=>'IndexMemory',
							mem_per_item=>1024,
							unit=>'KB',
							default=>18*1024)
			);

{
    foreach my $ver ($report->versions)
    {
	my $dm=0;
	my $im=0;
	while(my ($tname,$t)= $report->tables_each())
	{
	    $dm+=$t->dm_needed->{$ver};
	    $dm+=$t->vdm_needed->{$ver} || 0;
	    $im+=$t->im_needed->{$ver};
	}
	$report->parameters->{'DataMemory'}->value_set($ver => $dm/1024);
	$report->parameters->{'IndexMemory'}->value_set($ver => $im/1024);
    }
}


if($savequeries)
{
    open Q, "> $savequeries";
    print Q Dumper(\%queries);
    close Q;
}

use Data::Dumper;

if($debug)
{
    eval 'print STDERR Dumper($report)';
}

$format= "text" unless $format;

if($format eq 'text')
{
    my $text_out= new MySQL::NDB::Size::Output::Text($report);
    $text_out->output();
}
elsif($format eq 'html')
{
    my $html_out= new MySQL::NDB::Size::Output::HTML($report);
    $html_out->output();
}

package MySQL::NDB::Size::Output::Text;
use Data::Dumper;

sub new { bless { report=> $_[1] }, $_[0]}

sub ul
{
    my $s= $_[1]."\n";
    $s.='-' foreach (1..length($_[1]));
    return $s.="\n";
}

sub output
{
    my $self= shift;
    my $r= $self->{report};

    print $self->ul("ndb_size.pl report for ". $r->database().
		    " (".(($r->tables_count()||0)-($r->supporting_tables_count()||0)).
		    " tables)");

    print "Connected to: ".$r->dsn()."\n\n";

    print "Including information for versions: ".
	join(', ',@{$r->versions})."\n\n";

    foreach my $tname (@{$r->tables_keys()})
    {
	my $t= $r->tables->{$tname};
#	next if $r->supporting_tables_exists($tname);

	print $self->ul($tname)."\n";

	# format strings
	my $f= "%25s ";
	my $v= "%10s ";

	# Columns
	print "DataMemory for Columns (* means varsized DataMemory):\n";
	printf $f.'%20s %9s %5s','Column Name','Type','Varsized', 'Key';
	printf $v, $_ foreach @{$r->versions};
	print "\n";
	my %dm_totals;
	my %vdm_totals;
	while(my ($cname, $c)= $t->columns_each())
	{
	    $c->type =~ /^([^\(]*)/g;
	    printf $f.'%20s %9s %5s',
	    $cname,
	    $1.(
		 ( $c->size and not $c->type() =~ /(enum|set)/)
		 ? '('.$c->size.')'
		 :'' ),
	    ($c->is_varsize)? 'Y':' ',
	    (defined($c->Key))?$c->Key:' ';
	    foreach(@{$r->versions})
	    {
		if($c->ver_dm_exists($_))
		{
		    printf $v, $c->ver_dm($_).(($c->is_varsize)?'*':'');
		    if($c->is_varsize())
		    {
			$vdm_totals{$_}+= $c->ver_dm($_);
		    }
		    else
		    {
			$dm_totals{$_}+= $c->ver_dm($_);
		    }
		}
		else
		{
		    printf $v, $c->dm||'N/A';
		    $dm_totals{$_}+=$c->dm||0;
		}
	    }
	    print "\n";
	}
	printf $f.'%20s %9s %5s','','','', '';
	printf $v, '--' foreach @{$t->dm_versions};
	print "\n";
	printf $f.'%20s %9s %5s','Fixed Size Columns DM/Row','','','';
	printf $v, $dm_totals{$_} foreach @{$r->versions};
	print "\n";
	printf $f.'%20s %9s %5s','Varsize Columns DM/Row','','','';
	printf $v, $vdm_totals{$_} || 0 foreach @{$r->versions};
	print "\n";


	# DM for Indexes
	print "\n\nDataMemory for Indexes:\n";
	printf $f.'%20s ','Index Name','Type';
	printf $v, $_ foreach @{$r->versions};
	print "\n";
	my %idx_dm_totals;
	while(my ($iname, $i)= $t->indexes_each())
	{
	    printf $f.'%20s ',$iname,$i->type();
	    foreach(@{$r->versions})
	    {
		if($i->ver_dm_exists($_))
		{
		    printf $v, $i->ver_dm($_).(($i->is_varsize)?'*':'');
		    $idx_dm_totals{$_}+= $i->ver_dm($_);
		}
		else
		{
		    printf $v, ((defined($i->dm))?$i->dm:'N/A');
		    $idx_dm_totals{$_}+= $i->dm if defined($i->dm);
		}
	    }
	    print "\n";
	}
	printf $f.'%20s ','','';
	printf $v, '--' foreach @{$r->versions};
	print "\n";
	printf $f.'%20s ','Total Index DM/Row','';
	printf $v, (defined($idx_dm_totals{$_}))?$idx_dm_totals{$_}:0
	    foreach @{$r->versions};
	print "\n\n";

	if(@{$t->supporting_tables()})
	{
	    print "\n\nSupporting Tables DataMemory/Row";
	    my %supp_total;
	    foreach(@{$t->supporting_tables()})
	    {
		print "\n";
		printf $f, $_;
		my $st= $r->tables->{$_};
		printf $v, $st->row_dm_size->{$_} foreach @{$st->dm_versions};
		$supp_total{$_}+=$st->row_dm_size->{$_}
		foreach @{$st->dm_versions};
	    }
	    print "\n";
	    printf $f, '';
	    printf $v, '--' foreach @{$t->dm_versions};
	    print "\n";
	    printf $f, 'This DataMemory/Row';
	    printf $v, $t->row_dm_size->{$_} foreach @{$t->dm_versions};
	    $supp_total{$_}+=$t->row_dm_size->{$_}
	    foreach @{$t->dm_versions};
	    print "\n";
	    printf $f, 'Total DM/Row';
	    printf $v, $supp_total{$_} foreach @{$t->dm_versions};
	    print "  Includes DM in other tables\n";
	}

	# IM for Columns
	print "\n\nIndexMemory for Indexes:\n";
	printf $f,'Index Name';
	printf $v, $_ foreach @{$r->versions};
	print "\n";
	my %im_totals;
	foreach my $iname (@{$t->indexes_keys()})
	{
	    my $i= $t->indexes->{$iname};
	    next if $i->is_supporting_table();

	    printf $f, $iname;

	    foreach(@{$r->versions})
	    {
		if(!defined($i->im))
		{
		    printf $v,'N/A';
		    next;
		}
		if($i->ver_im_exists($_))
		{
		    printf $v, $i->ver_im->{$_};
		    $im_totals{$_}+= $i->ver_im->{$_};
		}
		else
		{
		    printf $v, $i->im;
		    $im_totals{$_}+=$i->im;
		}
	    }
	    print "\n";
	}
	printf $f,'';
	printf $v, '--' foreach @{$t->dm_versions};
	print "\n";
	printf $f,'Indexes IM/Row';
	printf $v, $im_totals{$_} foreach @{$r->versions};
	print "\n";

	if(@{$t->supporting_tables()})
	{
	    print "\n\nSupporting Tables IndexMemory/Row";
	    my %supp_total;
	    foreach(@{$t->supporting_tables()})
	    {
		print "\n";
		my $st= $r->tables->{$_};
		foreach(@{$st->indexes_keys()})
		{
		    printf $f, $st->schema().".".$st->name() if $_ eq 'PRIMARY';
		    printf $f, $st->schema().".".$st->name().$_ if $_ ne 'PRIMARY';
		    my $sti= $st->indexes->{$_};
		    printf $v, ($sti->ver_im_exists($_))
			?$sti->ver_im->{$_}
			:$sti->im() foreach @{$st->dm_versions};
		    $supp_total{$_}+= ($sti->ver_im_exists($_))
			?$sti->ver_im->{$_}
			:$sti->im() foreach @{$st->dm_versions};

		}
	    }
	    print "\n";
	    printf $f, '';
	    printf $v, '--' foreach @{$t->dm_versions};
	    print "\n";
	    print "\n";
	    printf $f, 'Total Suppt IM/Row';
	    printf $v, $supp_total{$_} foreach @{$t->dm_versions};
	    print "\n";
	}

	print "\n\n\nSummary (for THIS table):\n";
	printf $f, '';
	printf $v, $_ foreach @{$r->versions};
	print "\n";
	printf $f, 'Fixed Overhead DM/Row';
	printf $v, $t->row_dm_overhead->{$_} foreach @{$t->dm_versions};
	print "\n";
	printf $f, 'NULL Bytes/Row';
	printf $v, $t->dm_null_bytes->{$_}||0 foreach @{$t->dm_versions};
	print "\n";
	printf $f, 'DataMemory/Row';
	printf $v, $t->row_dm_size->{$_} foreach @{$t->dm_versions};
	print " (Includes overhead, bitmap and indexes)\n";

	print "\n";
	printf $f, 'Varsize Overhead DM/Row';
	printf $v, $t->row_vdm_overhead->{$_}||0 foreach @{$t->dm_versions};
	print "\n";
	printf $f, 'Varsize NULL Bytes/Row';
	printf $v, $t->vdm_null_bytes->{$_}||0 foreach @{$t->dm_versions};
	print "\n";
	printf $f, 'Avg Varsize DM/Row';
	printf $v, (exists($t->row_vdm_size->{$_})?
		    $t->row_vdm_size->{$_}: 0)
		    foreach @{$r->versions};
	print "\n\n";
	printf $f, 'No. Rows';
	printf $v, $t->rows foreach @{$r->versions};
	print "\n\n";
	printf $f, 'Rows/'.($t->dm_pagesize()/1024).'kb DM Page';
	printf $v, $t->dm_rows_per_page->{$_} foreach @{$r->versions};
	print "\n";
	printf $f, 'Fixedsize DataMemory (KB)';
	printf $v, $t->dm_needed->{$_}/1024 foreach @{$r->versions};
	print "\n\n";
	printf $f, 'Rows/'.($t->vdm_pagesize()/1024).'kb Varsize DM Page';
	printf $v, $t->vdm_rows_per_page->{$_}||0 foreach @{$r->versions};
	print "\n";
	printf $f, 'Varsize DataMemory (KB)';
	printf $v, ($t->vdm_needed->{$_}||0)/1024 foreach @{$r->versions};
	print "\n\n";
	printf $f, 'Rows/'.($t->im_pagesize()/1024).'kb IM Page';
	printf $v, $t->im_rows_per_page->{$_} foreach @{$r->versions};
	print "\n";
	printf $f, 'IndexMemory (KB)';
	printf $v, $t->im_needed->{$_}/1024 foreach @{$r->versions};

	print "\n\n\n";
    }

    print "\n\n\n";
    print $self->ul("Parameter Minimum Requirements");
    print "* indicates greater than default\n\n";
    printf "%25s  ","Parameter";
    printf "%15s ",'Default' ;
    printf "%15s%1s ",$_,'' foreach @{$r->versions};
    print "\n";
    while( my ($pname, $p)= $r->parameters_each())
    {
	printf "%25s  ",$pname.(($p->unit)?' ('.$p->unit.')':'');
	printf "%15u ", $p->default;
	printf "%15u%1s ", $p->value->{$_},
	  ($p->value->{$_} > $p->default)?'*':''
	      foreach @{$r->versions};
	print "\n";
    }
    print "\n\n\n";
}

sub table
{
    my $self= shift;
    my $t= shift;
}

package MySQL::NDB::Size::Output::HTML;

sub new { bless { report=> $_[1] }, $_[0]}

sub tag
{
    my ($self,$tag,$content)= @_;
    return "<$tag>$content</$tag>\n";
}

sub h1 { my ($self,$t)= @_; return $self->tag('h1',$t); }
sub h2 { my ($self,$t)= @_; return $self->tag('h2',$t); }
sub h3 { my ($self,$t)= @_; return $self->tag('h3',$t); }
sub h4 { my ($self,$t)= @_; return $self->tag('h4',$t); }

sub p { my ($self,$t)= @_; return $self->tag('p',$t); }
sub b { my ($self,$t)= @_; return $self->tag('b',$t); }

sub th
{
    my ($self)= shift;
    my $c;
    $c.=$self->tag('th',$_) foreach @_;
    return $self->tag('tr',$c);
}

sub tr
{
    my ($self)= shift;
    my $c;
    $c.=$self->tag('td',$_) foreach @_;
    return $self->tag('tr',$c);
}

sub td { my ($self,$t)= @_; return $self->tag('td',$t); }

sub ul
{
    my ($self)= shift;
    my $c;
    $c.= "  ".$self->li($_) foreach @_;
    return $self->tag('ul',$c);
}

sub li { my ($self,$t)= @_; return $self->tag('li',$t); }

sub href
{
    my ($self,$href,$t)= @_;
    $href =~ s/\$/__/g;
    return "<a href=\"$href\">$t</a>";
}

sub aname
{
    my ($self,$href,$t)= @_;
    $href =~ s/\$/__/g;
    return "<a id=\"$href\">$t</a>";
}

sub output
{
    my $self= shift;
    my $r= $self->{report};

    print <<ENDHTML;
    <!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.1//EN" "http://www.w3.org/TR/xhtml11/DTD/xhtml11.dtd">
	<html xmlns="http://www.w3.org/1999/xhtml" xml:lang="en">
	<head>
	<meta http-equiv="Content-Type" content="application/xhtml+xml; charset=utf-8"/>
	<meta name="keywords" content="MySQL Cluster" />
ENDHTML
print "<title>MySQL Cluster size estimate for ".$r->database()."</title>";
print <<ENDHTML;
	<style type="text/css">
	table   { border-collapse: collapse }
        td,th { border: 1px solid black }
        </style>
	</head>
<body>
ENDHTML

    print $self->h1("ndb_size.pl report for ". $r->database().
		    " (".(($r->tables_count()||0)-($r->supporting_tables_count()||0)).
		    " tables)");

    print $self->p("Connected to: ".$r->dsn());

    print $self->p("Including information for versions: ".
		   join(', ',@{$r->versions}));

    if(@{$r->tables_keys()})
    {
	print $self->h2("Table List");
	my @tlist;
	foreach(sort @{$r->tables_keys()})
	{
	    push @tlist, $self->href("#$_",$_);
	}
	print $self->ul(@tlist);
    }

    foreach my $tname (sort @{$r->tables_keys()})
    {
	my $t= $r->tables->{$tname};

	print $self->h2($self->aname($tname,$tname));

	# format strings
	my $f= "%25s ";
	my $v= "%10s ";

	# Columns
	print $self->h3("DataMemory for Columns");
	print $self->p("* means varsized DataMemory");
	print "<table>\n";
	print $self->th('Column Name','Type','Varsized', 'Key',
			@{$r->versions});

	my %dm_totals;
	my %vdm_totals;
	while(my ($cname, $c)= $t->columns_each())
	{
	    $c->type =~ /^([^\(]*)/g;
	    my @verinfo;
	    foreach(@{$r->versions})
	    {
		if($c->ver_dm_exists($_))
		{
		    push @verinfo, $c->ver_dm($_).(($c->is_varsize)?'*':'');
		    if($c->is_varsize())
		    {
			$vdm_totals{$_}+= $c->ver_dm($_);
		    }
		    else
		    {
			$dm_totals{$_}+= $c->ver_dm($_);
		    }
		}
		else
		{
		    push @verinfo, $c->dm||'N/A';
		    $dm_totals{$_}+=$c->dm||0;
		}
	    }

	    print $self->tr(
			    $cname,
			    $1.(
		 ( $c->size and not $c->type() =~ /(enum|set)/)
		 ? '('.$c->size.')'
		 :'' ),
	    ($c->is_varsize)? 'Y':' ',
	    (defined($c->Key))?$c->Key:' ',@verinfo);
	}

	{
	    my @dmtot;
	    push @dmtot, $self->b($dm_totals{$_}) foreach @{$r->versions};
	    print $self->tr($self->b('Fixed Size Columns DM/Row'),'','','',
			    @dmtot);

	}
	{
	    my @vdmtot;
	    push @vdmtot, $self->b($vdm_totals{$_} || 0)
		foreach @{$r->versions};
	    print $self->tr($self->b('Varsize Columns DM/Row'),'','','',
			    @vdmtot);
	}

	print "</table>\n";

	# DM for Indexes
	print $self->h3('DataMemory for Indexes');
	print "<table>\n";
	print $self->th('Index Name','Type',@{$r->versions});

	my %idx_dm_totals;
	while(my ($iname, $i)= $t->indexes_each())
	{
	    my @verinfo;
	    foreach(@{$r->versions})
	    {
		if($i->ver_dm_exists($_))
		{
		    push @verinfo, $i->ver_dm($_).(($i->is_varsize)?'*':'');
		    $idx_dm_totals{$_}+= $i->ver_dm($_);
		}
		else
		{
		    push @verinfo, ((defined($i->dm))?$i->dm:'N/A');
		    $idx_dm_totals{$_}+= $i->dm if defined($i->dm);
		}
	    }
	    printf $self->tr($iname,$i->type(),@verinfo);
	}
	{
	    my @idxtot;
	    push @idxtot, $self->b((defined($idx_dm_totals{$_}))
				   ? $idx_dm_totals{$_}:0)
		foreach @{$r->versions};
	    print $self->tr($self->b('Total Index DM/Row'),'',
			    @idxtot);
	}

	print "</table>";

	if(@{$t->supporting_tables()})
	{
	    print $self->h3("Supporting Tables DataMemory/Row");
	    my %supp_total;

	    print "<table>";
	    print $self->th('Table',@{$r->versions});
	    foreach(@{$t->supporting_tables()})
	    {
		my $st= $r->tables->{$_};
		my @stdm;
		push @stdm, $st->row_dm_size->{$_} foreach @{$st->dm_versions};

		print $self->tr($_,@stdm);

		$supp_total{$_}+=$st->row_dm_size->{$_}
		foreach @{$st->dm_versions};
	    }
	    {
		my @rdmtot;
		push @rdmtot, $self->b($t->row_dm_size->{$_})
		    foreach @{$t->dm_versions};
		print $self->tr($self->b('This DataMemory/Row'),@rdmtot);
	    }
	    $supp_total{$_}+=$t->row_dm_size->{$_}
	      foreach @{$t->dm_versions};

	    {
		my @tdmr;
		push @tdmr, $self->b($supp_total{$_})
		    foreach @{$t->dm_versions};
		print $self->tr($self->b('Total DM/Row (inludes DM in other tables)'),@tdmr);
	    }
	    print "</table>";
	}

	# IM for Columns
	print $self->h3("IndexMemory for Indexes");
	print "<table>\n";
	print $self->th('Index Name', @{$r->versions});

	my %im_totals;
	foreach my $iname (@{$t->indexes_keys()})
	{
	    my $i= $t->indexes->{$iname};
	    next if $i->is_supporting_table();

	    my @verinfo;
	    foreach(@{$r->versions})
	    {
		if(!defined($i->im))
		{
		    push @verinfo,'N/A';
		    next;
		}
		if($i->ver_im_exists($_))
		{
		    push @verinfo, $i->ver_im->{$_};
		    $im_totals{$_}+= $i->ver_im->{$_};
		}
		else
		{
		    push @verinfo, $i->im;
		    $im_totals{$_}+=$i->im;
		}
	    }
	    print $self->tr($iname, @verinfo);
	}
	{
	    my @v;
	    push @v, $self->b($im_totals{$_}) foreach @{$r->versions};
	    printf $self->tr('Indexes IM/Row',@v);
	}
	print "</table>\n";

	if(@{$t->supporting_tables()})
	{
	    print $self->h3("Supporting Tables IndexMemory/Row");
	    print "<table>\n";
	    my %supp_total;
	    foreach(@{$t->supporting_tables()})
	    {
		my $st= $r->tables->{$_};
		foreach(@{$st->indexes_keys()})
		{
		    my @r;
		    push @r, $st->schema().".".$st->name() if $_ eq 'PRIMARY';
		    push @r, $st->schema().".".$st->name().$_ if $_ ne 'PRIMARY';
		    my $sti= $st->indexes->{$_};
		    push @r, ($sti->ver_im_exists($_))
			?$sti->ver_im->{$_}
			:$sti->im() foreach @{$st->dm_versions};
		    $supp_total{$_}+= ($sti->ver_im_exists($_))
			?$sti->ver_im->{$_}
			:$sti->im() foreach @{$st->dm_versions};
		    print $self->tr(@r);
		}
	    }
	    {
		my @r;
		push @r, $self->b($supp_total{$_}) foreach @{$t->dm_versions};
		print $self->tr($self->b('Total Suppt IM/Row'),@r);
	    }
	    print "</table>\n";
	}

	print $self->h3("Summary (for THIS table)");
	print $self->h4("Fixed Sized Part");
	print "<table>\n";

	print $self->tr('',@{$r->versions});

	{   my @r;
	    push @r, $t->row_dm_overhead->{$_} foreach @{$t->dm_versions};
	    print $self->tr('Fixed Overhead DM/Row',@r);
	}
	{   my @r;
	    push @r, $t->dm_null_bytes->{$_}||0 foreach @{$t->dm_versions};
	    print $self->tr('NULL Bytes/Row',@r);
	}
	{   my @r;
	    push @r, $t->row_dm_size->{$_} foreach @{$t->dm_versions};
	    print $self->tr('DataMemory/Row (incl overhead, bitmap, indexes)',
			    @r);
	}
	print "</table>\n";
	print $self->h4("Variable Sized Part");
	print "<table>\n";

	{   my @r;
	    push @r, $t->row_vdm_overhead->{$_}||0 foreach @{$t->dm_versions};
	    print $self->tr('Varsize Overhead DM/Row',@r);
	}
	{   my @r;
	    push @r, $t->vdm_null_bytes->{$_}||0 foreach @{$t->dm_versions};
	    print $self->tr('Varsize NULL Bytes/Row',@r);
	}
	{   my @r;
	    push @r, (exists($t->row_vdm_size->{$_})?
		      $t->row_vdm_size->{$_}: 0)
		foreach @{$r->versions};
	    print $self->tr('Avg Varsize DM/Row',@r);
	}
	print "</table>\n";
	print $self->h4("Memory Calculations");
	print "<table>\n";

	{   my @r;
	    push @r, $t->rows foreach @{$r->versions};
	    print $self->tr('No. Rows',@r);
	}
	{   my @r;
	    push @r, $t->dm_rows_per_page->{$_} foreach @{$r->versions};
	    print $self->tr('Rows/'.($t->dm_pagesize()/1024).'kb DM Page',@r);
	}
	{   my @r;
	    push @r, $t->dm_needed->{$_}/1024 foreach @{$r->versions};
	    print $self->tr('Fixedsize DataMemory (KB)',@r);
	}
	{   my @r;
	    push @r, $t->vdm_rows_per_page->{$_}||0 foreach @{$r->versions};
	    print $self->tr('Rows/'.($t->vdm_pagesize()/1024).
			    'kb Varsize DM Page', @r);
	}
	{   my @r;
	    push @r, ($t->vdm_needed->{$_}||0)/1024 foreach @{$r->versions};
	    print $self->tr('Varsize DataMemory (KB)', @r);
	}
	{   my @r;
	    push @r, $t->im_rows_per_page->{$_} foreach @{$r->versions};
	    print $self->tr('Rows/'.($t->im_pagesize()/1024).'kb IM Page', @r);
	}
	{   my @r;
	    push @r, $t->im_needed->{$_}/1024 foreach @{$r->versions};
	    print $self->tr('IndexMemory (KB)', @r);
	}

	print "</table><hr/>\n\n";
    }

    print $self->h1("Parameter Minimum Requirements");
    print $self->p("* indicates greater than default");
    print "<table>\n";
    print $self->th("Parameter",'Default',@{$r->versions});
    while( my ($pname, $p)= $r->parameters_each())
    {
	my @r;
	push @r, $p->value->{$_}.
	    (($p->value->{$_} > $p->default)?'*':'')
	    foreach @{$r->versions};

	print $self->tr($pname.(($p->unit)?' ('.$p->unit.')':''),
			$p->default,
			@r);
    }
    print "</table></body></html>";
}

sub table
{
    my $self= shift;
    my $t= shift;
}
