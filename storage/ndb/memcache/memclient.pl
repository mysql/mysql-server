#! /usr/bin/env perl

# -*- cperl -*-

# Copyright (c) 2013, 2015, Oracle and/or its affiliates. 
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

################ TODO ################################
###  * Library improvements here should be ported back to mtr;
###    CMake should automatically copy in My/Memcache.pm as part of build
###  * Library should support explicit binary k/q commands with pipelining
###  * Implement TOUCH & GAT commands in library & utility (and server?)
###  * Support UDP
###  * Standardize library APIs to take (key, value, hashref-of-options)

use strict;
use Term::ReadLine;
use Getopt::Std;
use Term::ANSIColor qw(:constants);
use Text::Balanced qw(extract_quotelike);

our $VERSION = "1.0";
my $mc;      # Memcache connection

sub HELP_MESSAGE {
  my $fh = shift;
  print $fh "\n".
            "memclient [-a|-b] [host] [port] \n" .
            " -a: use ASCII protocol (default) \n" .
            " -b: use binary protocol \n" .
            "  host defaults to localhost; port defaults to 11211 \n\n";
}

$Getopt::Std::STANDARD_HELP_VERSION = 1;
our ($opt_a, $opt_b);
if(! getopts('ab')) { HELP_MESSAGE(\*STDOUT) ; exit 1 }
my $proto = $opt_b ? "binary" : "ASCII";
my $host = shift || "localhost";
my $port = shift || 11211;

### RUNTIME HELP
my %help = (
  "get"       => "<key> [ <key> ... ]",
  "delete"    => "<key>",
  "set"       => "<key> <value> [flags: <N> | expires: <N> ]",
  "add"       => "<key> <value> [flags: <N> | expires: <N> ]",
  "replace"   => "<key> <value> [flags: <N> | expires: <N> | cas: <N> ]",
  "flush_all" => "",
  "append"    => "<key> <text>",
  "prepend"   => "<key> <text>",
  "incr"      => "<key> <delta>",
  "decr"      => "<key> <delta>",
  "stats"     => "[stat-key]",
  "flags:"    => "<value>    -- Set default flags for storage operations",
  "expires:"  => "<value>    -- Set default expire time for storage operations",
  "quit"      => "quit memclient",
  "reconnect" => "reconnect to server"
);

if($opt_b)
{ # Binary Protocol only
}
else 
{ # ASCII Protocol only
  $help{"gets"} = "<key> [ <key> ... ]   -- ASCII GET with CAS";
}

sub help {
  my $response = "Commands:\n";
  $response .= sprintf("%s %-10s %s %s\n",BOLD, $_, RESET, $help{$_}) 
    for sort keys(%help);
  return $response;
}

### Set up readline
my $term = new Term::ReadLine 'memclient';
my $prompt = "memcache > ";

my $attribs = $term->Attribs;
$attribs->{completion_function} = sub {
  my ($text, $line, $start) = @_;
  return grep(/^$text/, qw(get gets delete set add replace flush_all stats 
                           incr decr append prepend expires: flags: cas: ));
};

my $OUT = $term->OUT || \*STDOUT;
print $OUT "Memclient $VERSION using " .  $term->ReadLine . "\n";


# Connect
$mc = $opt_b ? $mc = My::Memcache::Binary->new() : My::Memcache->new();
print "Attempting $proto connection to $host:$port ...\n";
my $r = $mc->connect($host, $port);
print ($r ? "Connected.\n" : "Connection failed.\n");
exit(1) unless($r);

### Main command loop
while ( defined ($_ = $term->readline($prompt)) ) {
  my $res = run_cmd($_);
  print $OUT $res, "\n" if($res);
  $term->addhistory($_) if /\S/;
}

### Run "get" and display result
sub run_get_cmd {
  return args_err("get") unless length($_[0]);
  my @keys = ($_[0]);
  push @keys, split(" ", $_[1]);
  my $value = $mc->get(@keys);
  my $with_cas = ( $proto eq "binary" || $mc->{has_cas} );
  return $mc->{error} if $mc->{error} ne "OK";

  ### Header line
  my $response = UNDERSCORE . "       KEY        | FLAGS |";
  $response .= $with_cas ? "       CAS      |VALUE\n" : "Value\n";
  $response .= RESET;

  ### Result lines
  while(my $r = $mc->next_result())
  {
    if($with_cas)
    {
      $response .= sprintf("%-18s| %-5u |%-16s|%s\n", 
                           $$r{key}, $$r{flags}, $$r{cas}, $$r{value});
    }
    else 
    {
      $response .= sprintf("%-18s| %-5u |%s\n", $$r{key}, $$r{flags}, $$r{value});
    }
  }
  return $response;
}

sub stats {
  my $arg = shift;
  my %stats = $mc->stats($arg);
  my $response = "";
  $response .= sprintf("%-35s %-35s\n", $_, $stats{$_}) for keys(%stats);
  return $response;
}

sub args_err {
  my $cmd = shift;
  return sprintf("USAGE: %s %s %s %s\n",BOLD, $cmd, RESET, $help{$cmd});
}

### Run a storage command (potentially with options)
sub run_storage_cmd {
  my ($cmd, $key, $argsX) = @_;
  my ($quoted, $value, $extra, $flags, $exp_time, $cas_chk);
  ($quoted, $extra, $value) = (extract_quotelike($argsX))[0,1,5];
  if($quoted) {  # unescape any escaped quote marks
    $value =~ s/\\\"/\"/g;   #"#
    $value =~ s/\\\'/\'/g;
  }
  else {  # no quotes
    ($value, $extra) = split(" ", $argsX, 2);
  }
  return args_err($cmd) unless length($value);
  while($extra =~ m/\G\W*(\w+:)\W+(\d+)/gc) {
    $flags = $2    if $1 eq "flags:";
    $exp_time = $2 if $1 eq "expires:";
    $cas_chk = $2  if($cmd eq "replace" && $1 eq "cas:");
  }
  $mc->store($cmd, $key, $value, $flags, $exp_time, $cas_chk);
  return $mc->{error};
}

sub run_math_cmd {
  my ($cmd, $key, $delta) = @_;
  return args_err($cmd) unless length($delta) && length($key);
  return $mc->$cmd($key, $delta);
}

sub run_quit_cmd {
  exit;
}

sub run_reconnect_cmd {
  my $r = $mc->connect($host, $port);
  return ($r ? "Connected.\n" : "Connection failed.\n");
}

sub run_cmd {
  my %storage_cmds = ("set"=>1,"add"=>1,"replace"=>1,"append"=>1,"prepend"=>1);
  my %math_cmds  = ("incr"=>1,"decr"=>1);

  my ($cmd,$arg1,$argsX) = split(" ",$_,3);
   SWITCH : for(lc($cmd)) {
    $mc->{get_with_cas} = 1                      if $_ eq "gets";
    return run_get_cmd($arg1, $argsX)            if m/gets?/;
    return run_storage_cmd($_, $arg1, $argsX)    if exists $storage_cmds{$_};
    return run_math_cmd($_, $arg1, $argsX)       if exists $math_cmds{$_};
    $mc->delete($arg1), return $mc->{error}      if $_ eq "delete";
    $mc->flush(), return $mc->{error}            if $_ eq "flush_all";
    return $mc->set_flags($arg1)                 if $_ eq "flags:";
    return $mc->set_expires($arg1)               if $_ eq "expires:";
    return stats($arg1)                          if $_ eq "stats";
    return run_quit_cmd()                        if $_ eq "quit";
    return run_reconnect_cmd()                   if $_ eq "reconnect";
    return help();
  }
}

