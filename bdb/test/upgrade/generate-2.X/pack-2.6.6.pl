#!/usr/bin/perl

use strict;
use Archive::Tar;

my $subdir;
my $file;
my $archive_name;

my $version = "2.6.6";
my $build_dir = "/work/db/upgrade/db-2.6.6/build_unix";
my $db_dump_path = "$build_dir/db_dump";
my $pwd = `pwd`;

$| = 1;

chomp( $pwd );

opendir( DIR, $version . "le" ) || die;
while( $subdir = readdir( DIR ) )
{
	if( $subdir !~ m{^\.\.?$} )
	{
		opendir( SUBDIR, $version . "le/$subdir" ) || die;
		while( $file = readdir( SUBDIR ) )
		{
			if( $file !~ m{^\.\.?$} )
			{
				print "[" . localtime() . "] " . "$subdir $file", "\n";

				eval
				{
                                        my $data;
                                        my $archive;

					system( "mkdir", "-p", "$version/$subdir" );
					$file =~ m{(.*)\.};
					$archive_name = "$1";
					$archive_name =~ s{Test}{test};
					$archive = Archive::Tar->new();
					$archive->add_data( "$archive_name-le.db",
						read_file( $version . "le/$subdir/$file" ) );
#					$archive->add_data( "$archive_name-be.db",
#						read_file( $version . "be/$subdir/$file" ) );
					$archive->add_data( "$archive_name.dump",
						db_dump( "$pwd/$version" . "le/$subdir/$file" ) );
					$data = tcl_dump( "$pwd/$version" . "le/$subdir/$file" );
					$archive->add_data( "$archive_name.tcldump", $data );
					$archive->write( "$version/$subdir/$archive_name.tar.gz", 9 );
				};
				if( $@ )
				{
					print( "Could not process $file: $@\n" );
				}
			}
		}
	}
}

sub read_file
{
	my ($file) = @_;
	my $data;

	open( FILE, "<$file" ) || die;
	read( FILE, $data, -s $file );
	close( file );

	return $data;
}

sub db_dump
{
	my ($file) = @_;

	#print $file, "\n";
	unlink( "temp.dump" );
	system( "sh", "-c", "$db_dump_path $file >temp.dump" ) && die;
	if( -e "temp.dump" )
	{
		return read_file( "temp.dump" );
	}
	else
	{
		die "db_dump failure: $file\n";
	}
}

sub tcl_dump
{
	my ($file) = @_;
        my $up_dump_args = "";

        if ($file =~ /test012/) {
               $up_dump_args .= "1";
        }

	unlink( "temp.dump" );
	open( TCL, "|$build_dir/dbtest" );
print TCL <<END;
cd $build_dir
source ../test/test.tcl
upgrade_dump $file $pwd/temp.dump $up_dump_args
END
	close( TCL );
	if( -e "temp.dump" )
	{
		return read_file( "temp.dump" );
	}
	else
	{
		die "TCL dump failure: $file\n";
	}
}
