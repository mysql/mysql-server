# -*- cperl -*-
# Copyright (c) 2011, 2012, Oracle and/or its affiliates. 
# All rights reserved.
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


########## Memcache Client Library for Perl
### 
###  $mc = My::Memcache->new()          create an ascii-protocol client 
###  $mc = My::Memcache::Binary->new()  create a binary-protocol client
###
###  $mc->connect(host, port)           returns 1 on success, 0 on failure
### 
###  $mc->{error}                       holds most recent error/status message
### 
###  $mc->set(key, value)               returns 1 on success, 0 on failure
###  $mc->add(key, value)               set if record does not exist
###  $mc->replace(key, value)           set if record exists
###  $mc->append(key, value)            append value to existing data
###  $mc->prepend(key, value)           prepend value to existing data
###  $mc->get(key)                      returns value or undef
###  $mc->delete(key)                   returns 1 on success, 0 on failure
###  $mc->stats(stat_key)               get stats; returns a hash
###  $mc->incr(key, amount)             returns the new value or undef
###  $mc->decr(key, amount)             like incr. (Note: In the Binary protocol
###                                     only, incr and decr can take a 3rd 
###                                     argument, the initial value). 
###  $mc->flush()                       flush_all
###
###  $mc->set_expires(sec)              Set TTL for all store operations
###  $mc->set_flags(int_flags)          Set numeric flags for store operations
###
###  $mc->note_config_version() 
###    Store the generation number of the running config in the filesystem,
###    for later use by wait_for_reconf()
### 
###  $mc->wait_for_reconf()
###    Wait for NDB/Memcache to complete online reconfiguration.  
###    Returns the generation number of the newly running configuration, 
###    or zero on timeout/error. 

use strict;
use lib 'lib';
use IO::Socket::INET;
use IO::File;
use Carp;
use mtr_report;            # for main::mtr_verbose()

require "mtr_process.pl";  # for mtr_ping_port() 
require "mtr_misc.pl";     # for mtr_milli_sleep()

package My::Memcache;

sub new {
  my $pkg = shift;
  bless { "created" => 1 , "error" => "" , "cf_gen" => 0,
          "exptime" => 0 , "flags" => 0
        }, $pkg;
}

sub connect {
  my $self = shift;
  my $host = shift;
  my $port = shift; 
  
  # Wait for memcached to be ready, up to ten seconds.
  my $retries = 100;
  while($retries && (::mtr_ping_port($port) == 0))
  {
     ::mtr_milli_sleep(100);
     $retries--;
  }

  my $conn = IO::Socket::INET->new(PeerAddr => "$host:$port", Proto => "tcp");
  if($conn) {
    $self->{connection} = $conn;
    $self->{connected} = 1;
    $self->{server} = "$host:$port";
    $self->{bin_req_id} = 0;
    return 1;
  }
  $self->{error} = "CONNECTION_FAILED";
  return 0;
}

sub DESTROY {
  my $self = shift;
  $self->{connection}->close();
}

sub note_config_version {
  my $self = shift;

  my $vardir = $ENV{MYSQLTEST_VARDIR};
  # Fetch the memcached current config generation number and save it
  my %stats = $self->stats("reconf");
  my $F = IO::File->new("$vardir/tmp/memcache_cf_gen", "w") or die;
  my $ver = $stats{"Running"};
  print $F "$ver\n";
  $F->close();

  $self->{cf_gen} = $ver;
}

sub set_expires {
  my $self = shift;
  my $delta = shift;
  
  $self->{exptime} = $delta;
}

sub set_flags {
  my $self = shift;
  my $flags = shift;
  
  $self->{flags} = $flags;
}

sub wait_for_reconf {
  my $self = shift;

  if($self->{cf_gen} == 0) { 
    my $cfgen = 0;
    my $vardir = $ENV{MYSQLTEST_VARDIR};
    my $F = IO::File->new("$vardir/tmp/memcache_cf_gen", "r");
    if(defined $F) {
      chomp($cfgen = <$F>);
      undef $F;
    }
    $self->{cf_gen} = $cfgen;
  }
  
  print STDERR "Config generation is : " . $self->{cf_gen} . "\n";
  my $wait_for = $self->{cf_gen} + 1 ;
  print STDERR "Waiting for: $wait_for \n";
  
  my $new_gen = $self->wait_for_config_generation($wait_for);
  if($new_gen > 0) {
    $self->{cf_gen} = $new_gen;
  }
  else {
    print STDERR "Timed out.\n";
  }
  
  return $new_gen;
}
  

# wait_for_config_generation($cf_gen)
# Wait until memcached is running config generation >= to $cf_gen
# Returns 0 on error/timeout, or the actual running generation number
#
sub wait_for_config_generation {
  my $self = shift;
  my $cf_gen = shift;
  my $ready = 0;
  my $retries = 100;   # 100 retries x 100 ms = 10s
  
  while($retries && ! $ready) {
    ::mtr_milli_sleep(100);     
    my %stats = $self->stats("reconf");
    if($stats{"Running"} >= $cf_gen) {
      $ready = $stats{"Running"};
    }
    else {
      $retries -= 1;
    }
  }
  return $ready;
}

  
sub delete {
  my $self = shift;
  my $key = shift;
  my $sock = $self->{connection}; 
  
  $sock->print("delete $key\r\n") || Carp::confess "send error";
  
  $self->{error} = $sock->getline();
  return $self->{error} =~ "^DELETED" ? 1 : $self->normalize_error();
}


sub _txt_store {
  my $self = shift;
  my $cmd = shift;
  my $key = shift;
  my $value = shift;
  my $sock = $self->{connection};
  
  $sock->printf("%s %s %d %d %d\r\n%s\r\n",$cmd, $key, 
                $self->{flags}, $self->{exptime}, length($value), $value);
  return $sock->getline();
}


sub set {
  my ($self, $key, $value) = @_;
  
  $self->{error} = $self->_txt_store("set", $key, $value);
  return $self->{error} =~ "^STORED" ? 1 : $self->normalize_error();
}


sub add {
  my ($self, $key, $value) = @_;
  
  $self->{error} = $self->_txt_store("add", $key, $value);
  return $self->{error} =~ "^STORED" ? 1 : $self->normalize_error();
}


sub append {
  my ($self, $key, $value) = @_;
  
  $self->{error} = $self->_txt_store("append", $key, $value);
  return $self->{error} =~ "^STORED" ? 1 : $self->normalize_error();
}


sub prepend {    
  my ($self, $key, $value) = @_;
  
  $self->{error} = $self->_txt_store("prepend", $key, $value);
  return $self->{error} =~ "^STORED" ? 1 : $self->normalize_error();
}


sub replace {
  my ($self, $key, $value) = @_;
  
  $self->{error} = $self->_txt_store("replace", $key, $value);
  return $self->{error} =~ "^STORED" ? 1 : $self->normalize_error();
}


sub get {
  my $self = shift;
  my $key = shift;
  my $sock = $self->{connection};
  
  $sock->print("get $key\r\n") || Carp::confess "send error";
  my $response = $sock->getline();
  my $val;
  
  if ($response =~ /^END/) 
  {
    $self->{error} = "NOT_FOUND";
    return undef;
  }
  else
  {
    $response =~ /^VALUE (.*) (\d+) (\d+)/;
    $self->{flags} = $2;
    my $len = $3;
    $sock->read($val, $len);
    $sock->getline();  # \r\n after value
    $sock->getline();  # END\r\n
    
    return $val;
  }
}


sub _txt_math {
  my ($self, $cmd, $key, $delta) = @_;
  my $sock = $self->{connection};
  
  $sock->print("$cmd $key $delta \r\n") || Carp::confess "send error";
  my $response = $sock->getline();
  my $val;
  
  if ($response =~ "^NOT_FOUND")
  {
    $self->{error} = "NOT_FOUND";
    return undef;
  }
  elsif ($response =~ "ERROR")
  {
    $self->{error} = $response;
    $self->normalize_error();
    return undef;
  }
  
  $response =~ /(\d+)/;
  return $1;
}


sub incr {
  my ($self, $key, $delta) = @_;
  return $self->_txt_math("incr", $key, $delta);
}


sub decr {
  my ($self, $key, $delta) = @_;
  return $self->_txt_math("decr", $key, $delta);
}


sub stats {
  my $self = shift;
  my $key = shift;
  my $sock = $self->{connection};

  $sock->print("stats $key\r\n") || Carp::confess "send error";
  
  $self->{error} = "OK";
  my %response = ();
  my $line = "";
  while($line !~ "^END") {
    if(($line) && ($line =~ /^STAT(\s+)(\S+)(\s+)(\S+)/)) {
      $response{$2} = $4;
    }
    $line = $sock->getline();
  }
  
  return %response;
}

sub flush {
  my $self = shift;
  my $key = shift;
  my $sock = $self->{connection}; 
  
  $sock->print("flush_all\r\n") || Carp::confess "send error";
  
  $self->{error} = $sock->getline();
  return $self->{error} =~ "^OK" ? 1 : $self->normalize_error();
}


# Try to provide consistent error messagees across ascii & binary protocols
sub normalize_error {
  my $self = shift;
  my %error_message = (
  "STORED\r\n"                         => "OK",
  "EXISTS\r\n"                         => "KEY_EXISTS",
  "CLIENT_ERROR value too big\r\n"     => "VALUE_TOO_LARGE",
  "SERVER_ERROR object too large for cache\r\n"     => "VALUE_TOO_LARGE",
  "CLIENT_ERROR invalid arguments\r\n" => "INVALID_ARGUMENTS",
  "SERVER_ERROR not my vbucket\r\n"    => "NOT_MY_VBUCKET",
  "SERVER_ERROR out of memory\r\n"     => "SERVER_OUT_OF_MEMORY",
  "SERVER_ERROR not supported\r\n"     => "NOT_SUPPORTED",
  "SERVER_ERROR internal\r\n"          => "INTERNAL_ERROR"
  );  
  my $norm_error = $error_message{$self->{error}};
  $self->{error} = $norm_error if(defined($norm_error));
  return 0;
} 

#  -----------------------------------------------------------------------
#  ------------------         BINARY PROTOCOL         --------------------
#  -----------------------------------------------------------------------

package My::Memcache::Binary;
@My::Memcache::Binary::ISA = "My::Memcache";

use constant BINARY_HEADER_FMT  => "CCnCCnNNNN";
use constant BINARY_REQUEST     => 0x80;
use constant BINARY_RESPONSE    => 0x81;

use constant BIN_CMD_GET        => 0x00;
use constant BIN_CMD_SET        => 0x01;
use constant BIN_CMD_ADD        => 0x02;
use constant BIN_CMD_REPLACE    => 0x03;
use constant BIN_CMD_DELETE     => 0x04;
use constant BIN_CMD_INCR       => 0x05;
use constant BIN_CMD_DECR       => 0x06;
use constant BIN_CMD_QUIT       => 0x07;
use constant BIN_CMD_FLUSH      => 0x08;
use constant BIN_CMD_NOOP       => 0x0A;
use constant BIN_CMD_APPEND     => 0x0E;
use constant BIN_CMD_PREPEND    => 0x0F;
use constant BIN_CMD_STAT       => 0x10;

my %error_message = (
 0x00 => "OK",
 0x01 => "NOT_FOUND",
 0x02 => "KEY_EXISTS", 
 0x03 => "VALUE_TOO_LARGE",
 0x04 => "INVALID_ARGUMENTS",
 0x05 => "NOT_STORED",
 0x06 => "NON_NUMERIC_VALUE",
 0x07 => "NOT_MY_VBUCKET",
 0x81 => "UNKNOWN_COMMAND",
 0x82 => "SERVER_OUT_OF_MEMORY",
 0x83 => "NOT_SUPPORTED",
 0x84 => "INTERNAL_ERROR",
 0x85 => "SERVER_BUSY",
 0x86 => "SERVER_TEMPORARY_ERROR"
);


sub send_binary_request {
  my $self = shift;
  my ($cmd, $key, $val, $extra_header) = @_;
  my $sock = $self->{connection};
  my $key_len    = length($key);
  my $val_len    = length($val);
  my $extra_len  = length($extra_header);
  my $total_len  = $key_len + $val_len + $extra_len;
  my $cas_hi     = 0;
  my $cas_lo     = 0;

  $self->{bin_req_id}++;
  
  my $header = pack(BINARY_HEADER_FMT, BINARY_REQUEST, $cmd,
                    $key_len, $extra_len, 0, 0, $total_len, 
                    $self->{bin_req_id}, $cas_hi, $cas_lo);
  my $packet = $header . $extra_header . $key . $val;

  $sock->send($packet) || Carp::confess "send failed";
}


sub get_binary_response {
  my $self = shift;
  my $sock = $self->{connection};
  my $header_len = length(pack(BINARY_HEADER_FMT));
  my $expected = $self->{bin_req_id};
  my $header;
  my $body="";

  $sock->recv($header, $header_len);

  my ($magic, $cmd, $key_len, $extra_len, $datatype, $status, $body_len,
      $opaque, $cas_hi, $cas_lo) = unpack(BINARY_HEADER_FMT, $header);
  
  ($magic == BINARY_RESPONSE) || Carp::confess "Bad magic number in response";
  ($opaque == $expected) || Carp::confess "Response out of order ($expected/$opaque)";
  
  while($body_len - length($body) > 0) {
    my $buf;
    $sock->recv($buf, $body_len - length($body));
    $body .= $buf;
  }
  $self->{error} = $error_message{$status};

  # Packet structure is: header .. extras .. key .. value 
  my $l = $extra_len + $key_len;
  my $extras = substr $body, 0, $extra_len;
  my $key    = substr $body, $extra_len, $key_len; 
  my $value  = substr $body, $l, $body_len - $l;

  return ($status, $value, $key, $extras);
}  


sub bin_math {
  my $self = shift;
  my ($cmd, $key, $delta, $initial) = @_;
  my $expires = 0xffffffff;  # 0xffffffff means the create flag is NOT set
  if(defined($initial))  { $expires = $self->{exptime};   }
  else                   { $initial = 0;                  }
  my $value = undef;
  
  my $extra_header = pack "NNNNN", 
  ($delta   / (2 ** 32)),   # delta hi
  ($delta   % (2 ** 32)),   # delta lo
  ($initial / (2 ** 32)),   # initial hi
  ($initial % (2 ** 32)),   # initial lo
  $expires;
  $self->send_binary_request($cmd, $key, '', $extra_header);

  my ($status, $packed_val) = $self->get_binary_response();
  if($status == 0) {
    my ($val_hi, $val_lo) = unpack("NN", $packed_val);
    $value = ($val_hi * (2 ** 32)) + $val_lo;
  }
  return $value;
}


sub bin_store {
  my $self = shift;
  my $cmd = shift;
  my $key = shift;
  my $value = shift;
  
  my $extra_header = pack "NN", $self->{flags}, $self->{exptime};
  $self->send_binary_request($cmd, $key, $value, $extra_header);
  
  my ($status) = $self->get_binary_response();
  return ($status == 0) ? 1 : 0;
}


sub get {
  my $self = shift;
  my $key = shift;
  
  $self->send_binary_request(BIN_CMD_GET, $key, '', '');  
  my ($status, $value) = $self->get_binary_response();
  return ($status == 0) ? $value : undef;
}

sub stats {
  my $self = shift;
  my $key = shift;
  my %response, my $status, my $value, my $klen, my $tlen;

  $self->send_binary_request(BIN_CMD_STAT, $key, '', '');
  do {
    ($status, $value, $key) = $self->get_binary_response();
    if($status == 0) {
      $response{$key} = $value;
    } 
  } while($status == 0 && $key);

  return %response;
}

sub flush {
  my ($self, $key, $value) = @_;
  $self->send_binary_request(BIN_CMD_FLUSH, $key, '', ''); 
  my ($status, $value) = $self->get_binary_response();
  return ($status == 0) ? 1 : 0;
}
  
sub set {
  my ($self, $key, $value) = @_;
  return $self->bin_store(BIN_CMD_SET, $key, $value);
}

sub add {
  my ($self, $key, $value) = @_;
  return $self->bin_store(BIN_CMD_ADD, $key, $value);
}

sub replace {
  my ($self, $key, $value) = @_;
  return $self->bin_store(BIN_CMD_REPLACE, $key, $value);
}

sub append {
  my ($self, $key, $value) = @_;
  $self->send_binary_request(BIN_CMD_APPEND, $key, $value, '');
  my ($status) = $self->get_binary_response();
  return ($status == 0) ? 1 : 0;
}

sub prepend {
  my ($self, $key, $value) = @_;
  $self->send_binary_request(BIN_CMD_PREPEND, $key, $value, '');
  my ($status) = $self->get_binary_response();
  return ($status == 0) ? 1 : 0;
}

sub delete { 
  my ($self, $key) = @_;
  $self->send_binary_request(BIN_CMD_DELETE, $key, '', '');
  my ($status, $value) = $self->get_binary_response();
  return ($status == 0) ? 1 : 0;
}
  
sub incr {
  my ($self, $key, $delta, $initial) = @_;
  return $self->bin_math(BIN_CMD_INCR, $key, $delta, $initial);
}

sub decr {
  my ($self, $key, $delta, $initial) = @_;
  return $self->bin_math(BIN_CMD_DECR, $key, $delta, $initial);
}


1;

