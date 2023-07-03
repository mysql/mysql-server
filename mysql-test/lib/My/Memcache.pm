# -*- cperl -*-
# Copyright (c) 2011, 2022, Oracle and/or its affiliates.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2.0,
# as published by the Free Software Foundation.
#
# This program is also distributed with certain software (including
# but not limited to OpenSSL) that is licensed under separate terms,
# as designated in a particular file or component or in included license
# documentation.  The authors of MySQL hereby grant you an additional
# permission to link the program and your derivative works with the
# separately licensed software that they have included with MySQL.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License, version 2.0, for more details.
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
###  $mc->store(cmd, key, value, ...)   alternate API for set/add/replace/append/prepend
###  $mc->set(key, value)               returns 1 on success, 0 on failure
###  $mc->add(key, value)               set if record does not exist
###  $mc->replace(key, value)           set if record exists
###  $mc->append(key, value)            append value to existing data
###  $mc->prepend(key, value)           prepend value to existing data
###
###  $mc->get(key, [ key ...])          returns a value or undef
###  $mc->next_result()                 Fetch results after get()
###
###  $mc->delete(key)                   returns 1 on success, 0 on failure
###  $mc->stats(stat_key)               get stats; returns a hash
###  $mc->incr(key, amount, [initial])  returns the new value or undef
###  $mc->decr(key, amount, [initial])  like incr.
###                                           The third argument is used in
###                                           the binary protocol ONLY.
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

################ TODO ################################
###  * Support explicit binary k/q commands with pipelining
###  * Implement TOUCH & GAT commands
###  * Support UDP
###  * Standardize APIs to take (key, value, hashref-of-options)

use strict;
use IO::Socket::INET;
use IO::File;
use Carp;
use Time::HiRes;
use Errno qw( EWOULDBLOCK );

######## Memcache Result

package My::Memcache::Result;

sub new {
  my ($pkg, $key, $flags, $cas) = @_;
  $cas = 0 if (!defined($cas));
  bless { "key"   => $key,
          "flags" => $flags,
          "cas"   => $cas,
          "value" => undef,
  }, $pkg;
}

######## Memcache Client

package My::Memcache;

sub new {
  my $pkg = shift;
  # min/max wait refer to msec. wait during temporary errors.  Both powers of 2.
  # io_timeout is in seconds (possibly fractional)
  # io_timeout * max_read_tries = read timeout
  bless { "created"         => 1,
          "error"           => "OK",
          "cf_gen"          => 0,
          "req_id"          => 0,
          "min_wait"        => 4,
          "max_wait"        => 8192,
          "temp_errors"     => 0,
          "total_wait"      => 0,
          "has_cas"         => 0,
          "flags"           => 0,
          "exptime"         => 0,
          "get_results"     => undef,
          "get_with_cas"    => 0,
          "failed"          => 0,
          "io_timeout"      => 5.0,
          "sysread_size"    => 512,
          "max_read_tries"  => 6,
          "readbuf"         => "",
          "buflen"          => 0,
          "error_detail"    => "",
          "read_try"        => 0,
          "max_write_tries" => 6
  }, $pkg;
}

sub summarize {
  my $val = shift;
  my $len = length $val;

  if ($len > 25) {
    return substr($val, 0, 10) . "..." . substr($val, -10) . " [len $len]";
  } else {
    return $val;
  }
}

# fail() is called when an MTR test fails
sub fail {
  my $self = shift;
  my $fd;

  if ($self->{failed}) {
    print STDERR " /// My::Memcache::fail() called recursively.\n";
    return;
  }
  $self->{failed} = 1;

  my $msg =
    "error: " .
    $self->{error} . "\t" . "read_try: " . $self->{read_try} . "\t" .
    "protocol: " . $self->protocol() . "\n" . "req_id: " . $self->{req_id} .
    "\t" . "temp err wait: " . $self->{total_wait} . " msec.\n";

  $msg .= "detail: " . $self->{error_detail} . "\n";
  $msg .= "buffer: " . summarize($self->{readbuf}) . "\n";

  my $r = $self->next_result();
  $msg .= "value: " . summarize($r->{value}) . "\n" if ($r);

  while (my $extra = shift) {
    $msg .= $extra;
  }
  $msg .= "\n";

  $msg .= $self->get_server_error_stats();

  # Load Average on linux
  $msg .= ("Load Avg: " . <$fd>) if (open($fd, "/proc/loadavg"));

  $msg .= "====~~~~____~~~~====\n";

  Carp::confess($msg);
}

# Attempt a new connection to memcached to flush the server's error log
# and obtain error statistics

sub get_server_error_stats {
  my $self       = shift;
  my $new_client = My::Memcache::Binary->new();
  my $r          = $new_client->connect($self->{host}, $self->{port});
  my $msg        = "";

  if ($r) {
    my %stats = $new_client->stats("errors");    # Also flushes server log
    $msg .= "Server error stats:\n";
    $msg .= sprintf("%s : %s\n", $_, $stats{$_}) for keys(%stats);
  } else {
    $msg =
      "Attempted new server connection to fetch error statistics but failed.\n";
  }
  return $msg;
}

# Common code to ASCII and BINARY protocols:

sub connect {
  my $self = shift;
  my $host = shift;
  my $port = shift;
  my $conn;

  # Wait for memcached to be ready, up to ten seconds.
  my $retries = 100;
  do {
    $conn = IO::Socket::INET->new(PeerAddr => "$host:$port", Proto => "tcp");
    if (!$conn) {
      Time::HiRes::usleep(100 * 1000);
      $retries--;
    }
  } while ($retries && !$conn);

  if ($conn) {
    $conn->blocking(0);    # Set non-blocking
    my $fd    = fileno $conn;
    my $fdset = '';
    vec($fdset, $fd, 1) = 1;
    $self->{fdset}      = $fdset;
    $self->{connection} = $conn;
    $self->{host}       = $host;
    $self->{port}       = $port;
    return 1;
  }
  $self->{error} = "CONNECTION_FAILED";
  return 0;
}

sub DESTROY {
  my $self = shift;
  if ($self->{connection}) {
    $self->{connection}->close();
  }
}

sub set_expires {
  my $self = shift;
  $self->{exptime} = shift;
}

sub set_flags {
  my $self = shift;
  $self->{flags} = shift;
}

# Some member variables are per-request.
# Clear them in preparation for a new request, and increment the request counter.
sub new_request {
  my $self = shift;
  $self->{error}    = "OK";
  $self->{read_try} = 0;
  $self->{has_cas}  = 0;
  $self->{req_id}++;
  $self->{get_results} = undef;
}

sub next_result {
  my $self = shift;
  shift @{ $self->{get_results} };
}

# note_config_version and wait_for_reconf are only for use by mysql-test-run
sub note_config_version {
  my $self = shift;

  my $vardir = $ENV{MYSQLTEST_VARDIR};
  # Fetch the memcached current config generation number and save it
  my %stats = $self->stats("reconf");
  my $F     = IO::File->new("$vardir/tmp/memcache_cf_gen", "w") or die;
  my $ver   = $stats{"Running"};
  print $F "$ver\n";
  $F->close();

  $self->{cf_gen} = $ver;
}

sub wait_for_reconf {
  my $self = shift;

  if ($self->{cf_gen} == 0) {
    my $cfgen  = 0;
    my $vardir = $ENV{MYSQLTEST_VARDIR};
    my $F      = IO::File->new("$vardir/tmp/memcache_cf_gen", "r");
    if (defined $F) {
      chomp($cfgen = <$F>);
      undef $F;
      unlink("$vardir/tmp/memcache_cf_gen");
    }
    $self->{cf_gen} = $cfgen;
  }

  my $wait_for = $self->{cf_gen} + 1;

  my $new_gen = $self->wait_for_config_generation($wait_for);
  if ($new_gen > 0) {
    $self->{cf_gen} = $new_gen;
  } else {
    print STDERR "Wait for config generation $wait_for timed out.\n";
  }

  return $new_gen;
}

# wait_for_config_generation($cf_gen)
# Wait until memcached is running config generation >= to $cf_gen
# Returns 0 on error/timeout, or the actual running generation number
#
sub wait_for_config_generation {
  my $self    = shift;
  my $cf_gen  = shift;
  my $ready   = 0;
  my $retries = 100;     # 100 retries x 100 ms = 10s

  while ($retries && !$ready) {
    Time::HiRes::usleep(100 * 1000);
    my %stats = $self->stats("reconf");
    if ($stats{"Running"} >= $cf_gen) {
      $ready = $stats{"Running"};
    } else {
      $retries -= 1;
    }
  }
  return $ready;
}

#  -----------------------------------------------------------------------
#  --------------        Low-level Network Handling        ---------------
#  -----------------------------------------------------------------------

# Utility function sets error based on network error & returns false.
sub socket_error {
  my $self   = shift;
  my $retval = shift;
  my $detail = shift;

  if ($retval == 1) {
    $self->{error} = "CONNECTION_CLOSED";
  } elsif ($retval == 0) {
    $self->{error} = "NETWORK_TIMEOUT";
  } else {
    $self->{error} = "NETWORK_ERROR: " . $!;
  }
  $self->{error_detail} = $detail if ($detail);

  return 0;
}

# $mc->write(packet).  Returns true on success, false on error.
sub write {
  my $self    = shift;
  my $packet  = shift;
  my $len     = length($packet);
  my $nsent   = 0;
  my $attempt = 0;
  my $r;

  if (!$self->{connection}->connected()) {
    return $self->socket_error(0, "write(): not connected");
  }

  while ($nsent < $len) {
    $r = select(undef, $self->{fdset}, undef, $self->{io_timeout});
    if ($r < 1) {
      if (++$attempt >= $self->{max_write_tries}) {
        return $self->socket_error($r, "write(): select() returned $r");
      }
    } else {
      $r = $self->{connection}->send(substr($packet, $nsent));
      if ($r > 0) {
        $nsent += $r;
      } elsif ($! != Errno::EWOULDBLOCK) {
        return $self->socket_error($r, "write(): send() errno $!");
      }
    }
  }
  return 1;
}

# $mc->read(desired_size).  Low-level read.  Returns true on success,
# appends data to readbuf, and sets buflen.  Returns false on error.
sub read {
  my $self   = shift;
  my $length = shift;
  my $sock   = $self->{connection};
  my $r;

  if ($length > 0) {
    $r = select($self->{fdset}, undef, undef, $self->{io_timeout});
    return $self->socket_error($r, "read(): select() $!") if ($r < 0);

    $r = $sock->sysread($self->{readbuf}, $length, $self->{buflen});
    if ($r > 0) {
      $self->{buflen} += $r;
    } elsif ($r < 0 && $! != Errno::EWOULDBLOCK) {
      return $self->socket_error($r == 0 ? 1 : $r, "read(): sysread() $!");
    }
  }
  return 1;
}

# Utility routine; assumes $len is available on buffer.
sub chop_from_buffer {
  my $self = shift;
  my $len  = shift;

  my $line = substr($self->{readbuf}, 0, $len);
  $self->{readbuf} = substr($self->{readbuf}, $len);
  $self->{buflen} -= $len;
  return $line;
}

# Returns a line if available; otherwise undef
sub get_line_from_buffer {
  my $self = shift;
  my $line = undef;

  my $idx = index($self->{readbuf}, "\r\n");
  if ($idx >= 0) {
    $line = $self->chop_from_buffer($idx + 2);    # 2 for \r\n
  }
  return $line;
}

# Returns length if available; otherwise undef
sub get_length_from_buffer {
  my $self = shift;
  my $len  = shift;

  if ($self->{buflen} >= $len) {
    return $self->chop_from_buffer($len);
  }
  return undef;
}

# Read up to newline.  Returns a line, or sets and returns error.
sub read_line {
  my $self = shift;
  my $message;

  $self->{read_try} = 0;
  while ((!defined($message)) && $self->{read_try} < $self->{max_read_tries}) {
    $self->{read_try}++;
    $message = $self->get_line_from_buffer();
    if (!defined($message)) {
      if (!$self->read($self->{sysread_size})) {
        return $self->{error};
      }
    }
  }
  if (defined($message)) {
    $self->normalize_error($message);    # Handle server error responses
    return $message;
  }

  $self->socket_error(0, "read_line(): timeout");
  return $self->{error};
}

# Read <length> bytes.  Returns the data, or returns undef and sets error.
sub read_known_length {
  my $self = shift;
  my $len  = shift;
  my $data;
  my $pre_buflen;

  $self->{read_try} = 0;
  while ($self->{read_try} < $self->{max_read_tries}) {
    $data = $self->get_length_from_buffer($len);
    return $data if (defined($data));
    # Desired length is not yet in buffer.
    $pre_buflen = $self->{buflen};
    if (!$self->read($len - $pre_buflen)) {  # limited by io_timeout
      return undef;   # read error
    }
    Carp::croak if ($self->{buflen} < $pre_buflen);
    if($self->{buflen} == $pre_buflen) {
      $self->{read_try}++;  # select() timed out. Nothing was read.
    }
  }
  # Perhaps the read completed on the final attempt
  $data = $self->get_length_from_buffer($len);
  if (!defined($data)) {
    $self->socket_error(0, "read_known_length(): timeout");
  }
  return $data;
}

#  -----------------------------------------------------------------------
#  ------------------          ASCII PROTOCOL         --------------------
#  -----------------------------------------------------------------------

sub protocol {
  return "ascii";
}

sub protocol_error {
  my $self   = shift;
  my $detail = shift;

  if ($self->{error} eq "OK") {
    $self->{error} = "PROTOCOL_ERROR";
  }
  if ($detail) {
    $self->{error_detail} = $detail;
  }
  return undef;
}

sub ascii_command {
  my $self     = shift;
  my $packet   = shift;
  my $waitTime = $self->{min_wait};
  my $maxWait  = $self->{max_wait};
  my $reply;

  do {
    $self->new_request();
    $self->write($packet);
    $reply = $self->read_line();
    if ($self->{error} eq "SERVER_TEMPORARY_ERROR" && $waitTime < $maxWait) {
      $self->{temp_errors} += 1;
      $self->{total_wait}  += (Time::HiRes::usleep($waitTime * 1000) / 1000);
      $waitTime *= 2;
    }
    } while ($self->{error} eq "SERVER_TEMPORARY_ERROR" &&
             $waitTime <= $maxWait);

  return $reply;
}

sub delete {
  my $self     = shift;
  my $key      = shift;
  my $response = $self->ascii_command("delete $key\r\n");
  return 1 if ($response =~ "^DELETED");
  return 0 if ($response =~ "^NOT_FOUND");
  return 0 if ($response =~ "^SERVER_ERROR");
  return $self->protocol_error("delete() got response: $response");
}

sub store {
  my ($self, $cmd, $key, $value, $flags, $exptime, $cas_chk) = @_;
  $flags   = $self->{flags}   unless $flags;
  $exptime = $self->{exptime} unless $exptime;
  my $packet;
  if (($cmd eq "cas" || $cmd eq "replace") && $cas_chk > 0) {
    $packet = sprintf("cas %s %d %d %d %d\r\n%s\r\n",
                      $key, $flags, $exptime, $cas_chk, length($value), $value);
  } else {
    $packet = sprintf("%s %s %d %d %d\r\n%s\r\n",
                      $cmd, $key, $flags, $exptime, length($value), $value);
  }
  my $response = $self->ascii_command($packet);
  return 1 if ($response =~ "^STORED");
  return 0 if ($response =~ "^NOT_STORED");
  return 0 if ($response =~ "^EXISTS");
  return 0 if ($response =~ "^NOT_FOUND");
  return 0 if ($response =~ "^SERVER_ERROR");
  return $self->protocol_error("store() got response: $response");
}

sub set {
  my ($self, $key, $value, $flags, $exptime) = @_;
  return $self->store("set", $key, $value, $flags, $exptime);
}

sub add {
  my ($self, $key, $value, $flags, $exptime) = @_;
  return $self->store("add", $key, $value, $flags, $exptime);
}

sub append {
  my ($self, $key, $value, $flags, $exptime) = @_;
  return $self->store("append", $key, $value, $flags, $exptime);
}

sub prepend {
  my ($self, $key, $value, $flags, $exptime) = @_;
  return $self->store("prepend", $key, $value, $flags, $exptime);
}

sub replace {
  my ($self, $key, $value, $flags, $exptime, $cas) = @_;
  return $self->store("replace", $key, $value, $flags, $exptime, $cas);
}

sub get {
  my $self = shift;
  my @results;
  my $keys = "";
  $keys .= shift(@_) . " " while (@_);
  my $command = $self->{get_with_cas} ? "gets" : "get";
  $self->{get_with_cas} = 0;    # CHECK, THEN RESET FOR NEXT CALL
  my $response = $self->ascii_command("$command $keys\r\n");
  return undef if ($self->{error} ne "OK");

  while ($response ne "END\r\n") {
    $response =~ /^VALUE (\S+) (\d+) (\d+) ?(\d+)?/;
    if (!(defined($1) && defined($2) && defined($3))) {
      return $self->protocol_error("GET response: $response");
    }
    my $result = My::Memcache::Result->new($1, $2, $4);
    my $value = $self->read_known_length($3);
    return undef if (!defined($value));
    $result->{value} = $value;
    $self->read_line();    # Get trailing \r\n after value
    $self->{has_cas} = 1 if ($4);
    push @results, $result;
    $response = $self->read_line();
  }
  $self->{get_results} = \@results;
  return $results[0]->{value} if @results;
  $self->{error} = "NOT_FOUND";
  return undef;
}

sub _txt_math {
  my ($self, $cmd, $key, $delta) = @_;
  my $response = $self->ascii_command("$cmd $key $delta \r\n");

  if ($response =~ "^NOT_FOUND" || $response =~ "ERROR") {
    return undef;
  }

  $response =~ /(\d+)/;
  return $self->protocol_error("MATH response: $response") unless defined($1);
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
  my $key = shift || "";

  $self->new_request();
  $self->write("stats $key\r\n");

  my %response = ();
  my $line     = $self->read_line();
  while ($line !~ "^END") {
    return %response if $line eq "ERROR\r\n";
    if (($line) && ($line =~ /^STAT(\s+)(\S+)(\s+)(\S+)/)) {
      $response{$2} = $4;
    } else {
      return $self->protocol_error("STATS response line: $line");
    }
    $line = $self->read_line();
  }

  return %response;
}

sub flush {
  my $self   = shift;
  my $key    = shift;
  my $result = $self->ascii_command("flush_all\r\n");
  return ($self->{error} eq "OK");
}

# Try to provide consistent error messagees across ascii & binary protocols
sub normalize_error {
  my $self  = shift;
  my $reply = shift;
  my %error_message = (
             "STORED\r\n"                                  => "OK",
             "EXISTS\r\n"                                  => "KEY_EXISTS",
             "NOT_FOUND\r\n"                               => "NOT_FOUND",
             "NOT_STORED\r\n"                              => "NOT_STORED",
             "CLIENT_ERROR value too big\r\n"              => "VALUE_TOO_LARGE",
             "SERVER_ERROR object too large for cache\r\n" => "VALUE_TOO_LARGE",
             "CLIENT_ERROR invalid arguments\r\n" => "INVALID_ARGUMENTS",
             "SERVER_ERROR not my vbucket\r\n"    => "NOT_MY_VBUCKET",
             "SERVER_ERROR out of memory\r\n"     => "SERVER_OUT_OF_MEMORY",
             "SERVER_ERROR not supported\r\n"     => "NOT_SUPPORTED",
             "SERVER_ERROR internal\r\n"          => "INTERNAL_ERROR",
             "SERVER_ERROR temporary failure\r\n" => "SERVER_TEMPORARY_ERROR");
  $self->{error} = $error_message{$reply} || "OK";
  return 0;
}

#  -----------------------------------------------------------------------
#  ------------------         BINARY PROTOCOL         --------------------
#  -----------------------------------------------------------------------

package My::Memcache::Binary;
BEGIN { @My::Memcache::Binary::ISA = qw(My::Memcache); }
use constant BINARY_HEADER_FMT => "CCnCCnNNNN";
use constant BINARY_REQUEST    => 0x80;
use constant BINARY_RESPONSE   => 0x81;

use constant BIN_CMD_GET     => 0x00;
use constant BIN_CMD_SET     => 0x01;
use constant BIN_CMD_ADD     => 0x02;
use constant BIN_CMD_REPLACE => 0x03;
use constant BIN_CMD_DELETE  => 0x04;
use constant BIN_CMD_INCR    => 0x05;
use constant BIN_CMD_DECR    => 0x06;
use constant BIN_CMD_QUIT    => 0x07;
use constant BIN_CMD_FLUSH   => 0x08;
use constant BIN_CMD_NOOP    => 0x0A;
use constant BIN_CMD_GETK    => 0x0C;
use constant BIN_CMD_GETKQ   => 0x0D;
use constant BIN_CMD_APPEND  => 0x0E;
use constant BIN_CMD_PREPEND => 0x0F;
use constant BIN_CMD_STAT    => 0x10;

sub protocol {
  return "binary";
}

sub error_message {
  my ($self, $code) = @_;
  my %error_messages = (0x00  => "OK",
                        0x01  => "NOT_FOUND",
                        0x02  => "KEY_EXISTS",
                        0x03  => "VALUE_TOO_LARGE",
                        0x04  => "INVALID_ARGUMENTS",
                        0x05  => "NOT_STORED",
                        0x06  => "NON_NUMERIC_VALUE",
                        0x07  => "NOT_MY_VBUCKET",
                        0x81  => "UNKNOWN_COMMAND",
                        0x82  => "SERVER_OUT_OF_MEMORY",
                        0x83  => "NOT_SUPPORTED",
                        0x84  => "INTERNAL_ERROR",
                        0x85  => "SERVER_BUSY",
                        0x86  => "SERVER_TEMPORARY_ERROR",
                        0x100 => "PROTOCOL_ERROR",
                        0x101 => "NETWORK_ERROR");
  return $error_messages{$code};
}

# Returns true on success, false on error
sub send_binary_request {
  my $self = shift;
  my ($cmd, $key, $val, $extra_header, $cas) = @_;

  $cas = 0 unless $cas;
  my $key_len   = length($key);
  my $val_len   = length($val);
  my $extra_len = length($extra_header);
  my $total_len = $key_len + $val_len + $extra_len;
  my $cas_hi    = ($cas >> 32) & 0xFFFFFFFF;
  my $cas_lo    = ($cas & 0xFFFFFFFF);

  $self->new_request();

  my $header = pack(BINARY_HEADER_FMT,
                    BINARY_REQUEST, $cmd,
                    $key_len, $extra_len, 0, 0, $total_len,
                    $self->{req_id}, $cas_hi, $cas_lo);
  my $packet = $header . $extra_header . $key . $val;

  return $self->write($packet);
}

sub get_binary_response {
  my $self       = shift;
  my $header_len = length(pack(BINARY_HEADER_FMT));
  my $header;
  my $body;

  $header = $self->read_known_length($header_len);
  return (0x101) if (!defined($header));

  my ($magic,  $cmd,      $key_len,  $extra_len, $datatype,
      $status, $body_len, $sequence, $cas_hi,    $cas_lo
  ) = unpack(BINARY_HEADER_FMT, $header);

  if ($magic != BINARY_RESPONSE) {
    $self->{error_detail} = "Magic number in response: $magic";
    return (0x100);
  }

  $body = $self->read_known_length($body_len);
  $self->{error} = $self->error_message($status);

  # Packet structure is: header .. extras .. key .. value
  my $cas    = ($cas_hi * (2**32)) + $cas_lo;
  my $l      = $extra_len + $key_len;
  my $extras = substr $body, 0, $extra_len;
  my $key    = substr $body, $extra_len, $key_len;
  my $value  = substr $body, $l, $body_len - $l;

  return ($status, $value, $key, $extras, $cas, $sequence);
}

sub binary_command {
  my $self = shift;
  my ($cmd, $key, $value, $extra_header, $cas) = @_;
  my $waitTime = $self->{min_wait};
  my $maxWait  = $self->{max_wait};
  my $status;
  my $wr;

  do {
    $wr = $self->send_binary_request($cmd, $key, $value, $extra_header, $cas);
    return undef unless $wr;
    ($status) = $self->get_binary_response();
    if ($status == 0x86 && $waitTime < $maxWait) {
      $self->{temp_errors} += 1;
      $self->{total_wait}  += (Time::HiRes::usleep($waitTime * 1000) / 1000);
      $waitTime *= 2;
    }
  } while ($status == 0x86 && $waitTime <= $maxWait);

  return ($status == 0) ? 1 : undef;
}

sub bin_math {
  my $self = shift;
  my ($cmd, $key, $delta, $initial) = @_;
  my $expires = 0xffffffff;    # 0xffffffff means the create flag is NOT set
  if   (defined($initial)) { $expires = $self->{exptime}; }
  else                     { $initial = 0; }
  my $value = undef;

  my $extra_header = pack "NNNNN", ($delta / (2**32)),    # delta hi
    ($delta % (2**32)),                                   # delta lo
    ($initial / (2**32)),                                 # initial hi
    ($initial % (2**32)),                                 # initial lo
    $expires;
  if ($self->send_binary_request($cmd, $key, '', $extra_header)) {
    my ($status, $packed_val) = $self->get_binary_response();
    if ($status == 0) {
      my ($val_hi, $val_lo) = unpack("NN", $packed_val);
      $value = ($val_hi * (2**32)) + $val_lo;
    }
  }
  return $value;
}

sub bin_store {
  my ($self, $cmd, $key, $value, $flags, $exptime, $cas) = @_;
  $flags   = $self->{flags}   unless $flags;
  $exptime = $self->{exptime} unless $exptime;
  my $extra_header = pack "NN", $flags, $exptime;

  return $self->binary_command($cmd, $key, $value, $extra_header, $cas);
}

## Pipelined multi-get
sub get {
  my $self = shift;
  my $idx  = $#_;              # Index of the final key
  my $cmd  = BIN_CMD_GETKQ;    # GET + KEY + NOREPLY
  my $wr;
  my $sequence = 0;
  my @results;

  for (my $i = 0 ; $i <= $idx ; $i++) {
    $cmd = BIN_CMD_GETK if ($i == $idx);    # Final request gets replies
    $wr = $self->send_binary_request($cmd, $_[$i], '', '');
  }
  return undef unless $wr;

  while ($sequence < $self->{req_id}) {
    my ($status, $value, $key, $extra, $cas);
    ($status, $value, $key, $extra, $cas, $sequence) =
      $self->get_binary_response();
    return undef if ($status > 0x01);
    if ($status == 0) {
      my $result = My::Memcache::Result->new($key, unpack("N", $extra), $cas);
      $result->{value} = $value;
      push @results, $result;
    }
  }
  $self->{get_results} = \@results;
  if (@results) {
    $self->{error} = "OK";
    return $results[0]->{value};
  }
  $self->{error} = "NOT_FOUND";
  return undef;
}

sub stats {
  my $self = shift;
  my $key  = shift;
  my %response, my $status, my $value, my $klen, my $tlen;

  $self->send_binary_request(BIN_CMD_STAT, $key, '', '');
  do {
    ($status, $value, $key) = $self->get_binary_response();
    if ($status == 0) {
      $response{$key} = $value;
    }
  } while ($status == 0 && $key);

  return %response;
}

sub flush {
  my ($self, $key, $value) = @_;
  $self->send_binary_request(BIN_CMD_FLUSH, $key, '', '');
  my ($status, $result) = $self->get_binary_response();
  return ($status == 0) ? 1 : 0;
}

sub store {
  my ($self, $cmd, $key, $value, $flags, $exptime, $cas) = @_;
  my %cmd_map = ("set"     => BIN_CMD_SET,
                 "add"     => BIN_CMD_ADD,
                 "replace" => BIN_CMD_REPLACE,
                 "append"  => BIN_CMD_APPEND,
                 "prepend" => BIN_CMD_PREPEND);
  return $self->bin_store($cmd_map{$cmd}, $key, $value, $flags, $exptime, $cas);
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
  return $self->binary_command(BIN_CMD_APPEND, $key, $value, '');
}

sub prepend {
  my ($self, $key, $value) = @_;
  return $self->binary_command(BIN_CMD_PREPEND, $key, $value, '');
}

sub delete {
  my ($self, $key) = @_;
  return $self->binary_command(BIN_CMD_DELETE, $key, '', '');
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
