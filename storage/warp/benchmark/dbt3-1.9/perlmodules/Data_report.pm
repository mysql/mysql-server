#!/usr/bin/perl -w
#
# Data_report.pm
#
# This file is released under the terms of the Artistic License.  Please see
# the file LICENSE, included in this package, for details.
#
# Copyright (C) 2003 Open Source Development Lab, Inc.
# Author: Jenny Zhang
#
package Data_report;

use strict;
use English;
use FileHandle;
use vars qw(@ISA @EXPORT);
use Exporter;
use CGI qw(:standard *table start_ul :html3);

=head1 NAME

extract_columns()

=head1 SYNOPSIS

extract columns from input file.  
the separator is spaces

=head1 ARGUMENTS

 -infile < the input file >
 -outfile < the output file name >
 -column < the columns to extract >

=cut

@ISA = qw(Exporter);
@EXPORT = qw(extract_columns extract_rows extract_columns_rows get_header 
		extract_columns_rows_sar convert_time_format get_os_version
		get_iostat_version get_sar_version get_vmstat_version
		get_max_row_number get_max_col_value gen_html_table
		get_sapdb_version convert_to_seconds);

sub extract_columns
{
	my ( $infile, $outfile, @column ) = @_;
	my ($fin, $fout, @data); 
	
	$fin = new FileHandle;		
	unless ( $fin->open( "< $infile" ) ) 
		{ die "can't open file $infile: $!"; }

	$fout = new FileHandle;		
	unless ( $fout->open( "> $outfile" ) ) 
		{ die "can't open file $outfile: $!"; }

	while (<$fin>)
	{
		chop;
		@data = split /\s+/;
		for (my $i=0; $i<$#column; $i++)
		{
			print $fout "$data[$column[$i]] ";
		}
		# the last column
		print $fout "$data[$column[$#column]]\n";
	}
	close($fin);
	close($fout);
}

=head1 NAME

extract_rows()

=head1 SYNOPSIS

extract rows from input file.  The output format can be cvs or gnuplot data

=head1 ARGUMENTS

 -infile < the input file >
 -outfile < the output file name, the output file will be:
         [name].dat: gnuplot data file
         [name].csv: csv file >
 -key < the lines extracted must contain key >
 -format < the format is either csv or gnuplot data >

=cut

sub extract_rows
{
	my ( $infile, $outfile, $key, $comment_key, $format ) = @_;
	my ($fin, $fout, @data, $comment_flag, $lcnt); 
	
	$fin = new FileHandle;		
	unless ( $fin->open( "< $infile" ) ) 
		{ die "can't open file $infile: $!"; }

	$fout = new FileHandle;		
	unless ( $fout->open( "> $outfile" ) ) 
		{ die "can't open file $outfile: $!"; }

	$comment_flag = 0;
	$lcnt = 1;

	while (<$fin>)
	{
		chop;
		@data = split /\s+/;
		if ( $format eq 'csv' )
		{
			# comment is printed once
			if ( /$comment_key/ && $comment_flag == 0 )
			{
				@data = split /\s+/;
				print $fout join(',', @data), "\n";
				$comment_flag = 1;
			}
			elsif ( /^$key$/ )
			{
				print $fout join(',', @data), "\n";
			}
		}
		elsif ( $format eq 'gnuplot' )
		{
			# comment is printed once
			if ( /$comment_key/ && $comment_flag == 0 )
			{
				print $fout '#';
				print $fout join(' ', @data), "\n";
				$comment_flag = 1;
			}
			elsif ( /^$key$/ )
			{
				print $fout "$lcnt ";
				print $fout join(' ', @data), "\n";
				$lcnt++;
			}
		}
	}
	close($fin);
	close($fout);
}

=head1 NAME

extract_columns_rows()

=head1 SYNOPSIS

extract rows and columns from input file.  
The output format can be cvs or gnuplot data

=head1 ARGUMENTS

 -infile < the input file >
 -outfile < the output file name, the output file will be:
         [name].dat: gnuplot data file
         [name].csv: csv file >
 -key < the lines extracted must contain key >
 -comment_key < if a line has comment_key then it is printed as comment >
 -comment < comment the the beginning of the file >
 -columns < columns to be extracted >
 -format < the format is either csv or gnuplot data >

=cut

sub extract_columns_rows
{
	my ( $infile, $outfile, $key, $comment_key, $comment, $format, @column ) = @_;
	my ($finput, $foutput, @data, $comment_flag, $lcnt, $sum, $first_line); 
	
	$finput = new FileHandle;		
	unless ( $finput->open( "< $infile" ) ) 
		{ die "can't open file $infile: $!"; }

	$foutput = new FileHandle;		
	unless ( $foutput->open( "> $outfile" ) ) 
		{ die "can't open file $outfile: $!"; }

	$comment_flag = 0;
	$lcnt = 0;
	$sum = 0;
	$first_line = 1;
	if ( $comment )
	{
		print $foutput "# $comment\n";
	}

	while (<$finput>)
	{
		chop;
		# get rid of leading spaces
		s/^\s+//;
		# skit empty line
		next if (/^$/);
		@data = split /\s+/;
		if ( $format eq 'csv' )
		{
			# comment is printed once
			if ( $comment_key && $comment_flag == 0 )
			{
				if ( /^$comment_key/ )
				{
					for (my $i=0; $i<$#column; $i++)
					{
						print $foutput "$data[$column[$i]],";
					}
					# the last column
					print $foutput "$data[$column[$#column]]\n";
					$comment_flag = 1;
				}
			}
			if ( $data[0] =~ /^$key$/ )
			{
				if ( $first_line )
				{
					$first_line = 0;
					next;
				}

				for (my $i=0; $i<$#column; $i++)
				{
					print $foutput "$data[$column[$i]],";
					$sum += $data[$column[$i]];
				}
				# the last column
				print $foutput "$data[$column[$#column]]\n";
				$sum += $data[$column[$#column]];
			}
		}
		elsif ( $format eq 'gnuplot' )
		{
			# comment is printed once
			if ( $comment_key && $comment_flag == 0 )
			{
				if ( /^$comment_key/ )
				{
					print $foutput '#';
					for (my $i=0; $i<$#column; $i++)
					{
						print $foutput "$data[$column[$i]] ";
					}
					# the last column
					print $foutput "$data[$column[$#column]]\n";
					$comment_flag = 1;
				}
			}
			if ( $data[0] =~ /^$key$/ )
			{
				if ( $first_line )
				{
					$first_line = 0;
					next;
				}
				print $foutput "$lcnt ";
				for (my $i=0; $i<$#column; $i++)
				{
					print $foutput "$data[$column[$i]] ";
					$sum += $data[$column[$i]];
				}
				# the last column
				print $foutput "$data[$column[$#column]]\n";
				$sum += $data[$column[$#column]];
				$lcnt++;
			}
		}
		else
		{
			die "invalid format $format\n";
		}
	}
	close($finput);
	close($foutput);
}


=head1 NAME

get_header()

=head1 SYNOPSIS

read key file and return headers

=head1 ARGUMENTS

 -infile < the input file >
 -option < the application option >
 -type < ap or hr >

=cut
sub get_header
{
	my ($infile, $option, $type) = @_;

	my ( @retstr, $line, $nf);
	my $fkey = new FileHandle;
	unless ( $fkey->open( "< $infile" ) ) { die "can not open file $infile: $!";}
	$nf = 1;
	while ( ( $line = $fkey->getline ) && ( $nf == 1 ) ) {
		next if ( $line =~ /^#/ || $line =~ /^\s*$/ );
		chomp $line;
		@retstr = split /;/, $line;
		if ( ( $retstr[ 0 ] eq $option ) && ( $retstr[ 1 ] eq $type ) )
		{
			$nf = 0;
			shift @retstr;    # Remove option
			shift @retstr;    # Remove type
		}
	}
	$fkey->close;
	if ( $nf == 1 ) { die "did not find line starting with $option;$type"; }
	return @retstr;
}

=head1 SYNOPSIS

extract rows and columns from input file.  
The output format can be cvs or gnuplot data

=head1 ARGUMENTS

 -infile < the input file >
 -outfile < the output file name, the output file will be:
         [name].dat: gnuplot data file
         [name].csv: csv file >
 -key < the lines extracted must contain key >
 -comment_key < if a line has comment_key then it is printed as comment >
 -comment < comment the the beginning of the file >
 -columns < columns to be extracted >
 -format < the format is either csv or gnuplot data >

=cut

sub extract_columns_rows_sar
{
	my ( $infile, $outfile, $key, $comment_key, $comment, $format, $start_column, @column ) = @_;
	my ($finput, $foutput, @data, $comment_flag, $lcnt, $sum); 
	
	open(SARIN, $infile) ||  die "can't open file $infile: $!"; 

	$foutput = new FileHandle;		
	unless ( $foutput->open( "> $outfile" ) ) 
		{ die "can't open file $outfile: $!"; }

	$comment_flag = 0;
	$lcnt = 0;
	$sum = 0;
	if ( $comment )
	{
		print $foutput "# $comment\n";
	}

	while (<SARIN>)
	{
		chop;
		# get rid of leading spaces
		s/^\s+//;
		# skit empty line
		next if (/^$/);
		@data = split /\s+/;
		
		# skip short lines
		next if ( $#data < $start_column || /^Linux/ ) ;
		if ( $format eq 'csv' )
		{
			# comment is printed once
			if ( $comment_key && $comment_flag == 0 )
			{
				if ( /^$comment_key/ )
				{
					for (my $i=0; $i<$#column; $i++)
					{
						print $foutput "$data[$column[$i]],";
					}
					# the last column
					print $foutput "$data[$column[$#column]]\n";
					$comment_flag = 1;
				}
			}
			if ( $data[$start_column] =~ /^$key/ )
			{
				for (my $i=0; $i<$#column; $i++)
				{
					print $foutput "$data[$column[$i]],";
					$sum += $data[$column[$i]];
				}
				# the last column
				print $foutput "$data[$column[$#column]]\n";
				$sum += $data[$column[$#column]];
			}
		}
		elsif ( $format eq 'gnuplot' )
		{
			# comment is printed once
			if ( $comment_key && $comment_flag == 0 )
			{
				if ( /^$comment_key/ )
				{
					print $foutput '#';
					for (my $i=0; $i<$#column; $i++)
					{
						print $foutput "$data[$column[$i]] ";
					}
					# the last column
					print $foutput "$data[$column[$#column]]\n";
					$comment_flag = 1;
				}
			}
			if ( $data[$start_column] =~ /^$key/ )
			{
				if ( ! ($data[0] eq "Average:") )
				{
				print $foutput "$lcnt ";
				for (my $i=0; $i<$#column; $i++)
				{
					print $foutput "$data[$column[$i]] ";
					$sum += $data[$column[$i]];
				}
				# the last column
				print $foutput "$data[$column[$#column]]\n";
				$sum += $data[$column[$#column]];
				$lcnt++;
				}
			}
		}
		elsif ( $format eq 'txt' )
		{
			$sum = 1;
			print $foutput "$_\n";
		}
		else
		{
			die "invalid format $format\n";
		}
	}
	close(SARIN);
	close($foutput);
}

sub convert_time_format
{
	my ($num_seconds) = @_;

        my ($h, $m, $s, $tmp_index, @ret);
	$h = $num_seconds/3600;
	#find the maximum integer that is less than $h
	for ( $tmp_index=1; $tmp_index<$h; $tmp_index++ ) {};
	$h = $tmp_index - 1;
	$num_seconds = $num_seconds - $h*3600;
	$m = $num_seconds/60;
	#find the maximum integer that is less than $h
	for ( $tmp_index=1; $tmp_index<$m; $tmp_index++ ) {};
	$m = $tmp_index - 1;
	$s = $num_seconds - $m*60;
	$ret[0] = $h;
	$ret[1] = $m;
	$ret[2] = $s;

	return @ret;
}

sub get_os_version
{
	my $line = `uname -a`;
	chop $line;
	my @str = split / /, $line;
	for (my $i=0; $i<=$#str; $i++)
	{
		if ($str[$i] =~ /[0-9]+\.[0-9]+\.[0-9]+/)
		{
			return $str[$i];
		}
	}
	die "did not find kernel info";
}

# get iostat version info so that we know whick key file to look at
sub get_iostat_version {
	my $str = `iostat -V 2>&1 | head -1 `;
	chomp $str;
	my @outline = split / /, $str;
	return $outline[ 2 ];
}

# get sar version info so that we know whick key file to look at
sub get_sar_version {
	my $str = `sar -V 2>&1 | head -1 `;
	chomp $str;
	my @outline = split / /, $str;
	return $outline[ 2 ];
}

sub get_vmstat_version
{
	my $str = `vmstat -V 2>&1 `;
	chomp $str;
	my @outline = split / /, $str;
	return $outline[ 2 ];
}

# read a bunch of files and return the max row number
sub get_max_row_number
{
	my ( @filelist ) = @_;
	my ( $max_row, $fin );
	$fin = new FileHandle;
	$max_row = 0;
	for ( my $i=0; $i<=$#filelist; $i++ ) 
	{
		print "file is $filelist[$i]\n";
		unless ( $fin->open( "< $filelist[$i]" ) )   { die "No data file $!"; }
		while ( <$fin> )
		{
			next if ( /^#/ || /^$/ );
			chop;
			my @value = split / /;
			my $cur_row = $value[0];
			if ( $cur_row > $max_row )	
			{
				$max_row = $cur_row;	
			}
		}
		close( $fin );
	}
	return $max_row;
}

# read a bunch of files and return the maximum value for column
sub get_max_col_value
{
	my ( $col, @filelist ) = @_;
	my ( $max_value, $fin );
	$fin = new FileHandle;

	# the calling script start the column from ZERO
	# but it starts from ONE due to to row number column 
	$col = $col + 1;
	$max_value = 0;
	for ( my $i=0; $i<=$#filelist; $i++ ) 
	{
		unless ( $fin->open( "< $filelist[$i]" ) )   { die "No data file $!"; }
		while ( <$fin> )
		{
			next if ( /^#/ || /^$/ );
			chop;
			my @value = split /\s+/;
			my $cur_value = $value[$col];
			if ( $cur_value > $max_value )	
			{
				$max_value = $cur_value;	
			}
		}
		close( $fin );
	}
	return $max_value;
}


# put a list of files into a table
sub gen_html_table
{
	my ($outfile, @filelist) = @_;
	
	my $fout = new FileHandle;
	unless ( $fout->open( "> $outfile" ) ) { die "cannot open output $!"; }
	
	#print $fout "<table>";
	print $fout start_table;
	for (my $i=0; $i<=$#filelist; $i++)
	{
		print $fout "<tr><td> <img src=\"$filelist[$i]\"> </td></tr>\n";
	}
	print $fout end_table;

	close($fout);
}

sub get_sapdb_version
{
	my ($fp);
	$fp = new FileHandle;
	unless ( $fp->open( "dbmcli dbm_version 2>&1 |" ) ) 
		{ die "cannot open command dbmcli $!"; }
	while (<$fp>)
	{
		next if ( !/VERSION/ );
		chop;
		my @outline = split /=/;
		close($fp);
		return $outline[ 1 ];
	}
	die "did not find sapdb version information";
}

sub convert_to_seconds
{
	my ($input_time, $output_time) = @_;
	
	# get execution time for the throughput test
	my (@day_fields, @time_fields);
	# the format is '2 days 22:07:30.292698'
	chop($input_time);
	if ( $input_time =~ /days/ )
	{
		@day_fields=split / /, $input_time;
		$input_time =~ s/(\d+) days //;
		if ( $day_fields[0] ne "")
		{
			#print "day fields is ", $day_fields[0];
			$output_time = 24*$day_fields[0]*60*60;
		}
	}

	@time_fields=split /:/, $input_time;
	# chop the miliseconds
	$time_fields[$#time_fields] =~ s/\..*//;
	$output_time = 0;

	#if ( $day_fields[0] ne "") 
	#{ 
		#print "day fields is ", $day_fields[0];  
	#	$output_time = 24*$day_fields[0]*60*60;
	#}
	$output_time += 60*60*$time_fields[0];
	$output_time += 60*$time_fields[1];
	$output_time += $time_fields[2];

	return $output_time;
}
1;
