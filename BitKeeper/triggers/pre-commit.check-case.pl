#!/usr/bin/perl
my $status = 0;

my $pending = $ENV{'BK_PENDING'};
exit 0 unless -f $pending;

open FI, "<", $pending || exit 0;
while(<FI>) {
	my ($file, $stuff) = split /\|/, $_, 2;
	next unless -f $file;
	$file =~ s/^(.*)\/([^\/]*)$/$2/;
	my $path = $1;
	opendir DIR, $path;
	my @files = sort map { lc } readdir DIR;
	closedir DIR;
	my %count = ();
	$count{$_}++ for @files;
	@files = grep { $count{$_} > 1 } keys %count;
	if(@files > 0) {
		print "$path/$file: duplicate file names: " . (join " ", @files) . "\n";
		$status = 1;
	}
}
close FI;

exit $status;
