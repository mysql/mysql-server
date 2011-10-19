
package Net::HandlerSocket;

use strict;
use warnings;

require Exporter;

our @ISA = qw(Exporter);

# Items to export into callers namespace by default. Note: do not export
# names by default without a very good reason. Use EXPORT_OK instead.
# Do not simply export all your public functions/methods/constants.

# This allows declaration	use Net::HandlerSocket ':all';
# If you do not need this, moving things directly into @EXPORT or @EXPORT_OK
# will save memory.
our %EXPORT_TAGS = ( 'all' => [ qw(
	
) ] );

our @EXPORT_OK = ( @{ $EXPORT_TAGS{'all'} } );

our @EXPORT = qw(
	
);

our $VERSION = '0.01';

require XSLoader;
XSLoader::load('Net::HandlerSocket', $VERSION);

# Preloaded methods go here.

1;
__END__
# Below is stub documentation for your module. You'd better edit it!

=head1 NAME

Net::HandlerSocket - Perl extension for blah blah blah

=head1 SYNOPSIS

  use Net::HandlerSocket;
  my $hsargs = { host => 'localhost', port => 9999 };
  my $cli = new Net::HandlerSocket($hsargs);
  $cli->open_index(1, 'testdb', 'testtable1', 'PRIMARY', 'foo,bar,baz');
  $cli->open_index(2, 'testdb', 'testtable2', 'i2', 'hoge,fuga');
  $cli->execute_find(1, '>=', [ 'aaa', 'bbb' ], 5, 100);
	# select foo,bar,baz from testdb.testtable1
	# 	where pk1 = 'aaa' and pk2 = 'bbb' order by pk1, pk2
	# 	limit 100, 5

=head1 DESCRIPTION

Stub documentation for Net::HandlerSocket, created by h2xs.

=head1 AUTHOR

Akira HiguchiE<lt>higuchi dot akira at dena dot jpE<gt>

=head1 COPYRIGHT AND LICENSE

Copyright (C) 2010 DeNA Co.,Ltd.. All rights reserved.
See COPYRIGHT.txt for details.

=cut
