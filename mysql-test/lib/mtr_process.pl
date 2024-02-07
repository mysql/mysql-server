# -*- cperl -*-
# Copyright (c) 2004, 2024, Oracle and/or its affiliates.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2.0,
# as published by the Free Software Foundation.
#
# This program is designed to work with certain software (including
# but not limited to OpenSSL) that is licensed under separate terms,
# as designated in a particular file or component or in included license
# documentation.  The authors of MySQL hereby grant you an additional
# permission to link the program and your derivative works with the
# separately licensed software that they have either included with
# the program or referenced in the documentation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License, version 2.0, for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

# This is a library file used by the Perl version of mysql-test-run,
# and is part of the translation of the Bourne shell script with the
# same name.

use strict;
use if IS_WINDOWS, "Net::Ping";

use Errno;
use Socket;

use My::Platform;

# Ancient perl might not have port_number method for Net::Ping.
# Check it and use fallback to connect() if it is not present.
BEGIN {
  my $use_netping = 0;
  if (IS_WINDOWS) {
    my $ping = Net::Ping->new();
    if ($ping->can("port_number")) {
      $use_netping = 1;
    }
  }
  eval 'sub USE_NETPING { $use_netping }';
}

sub mtr_ping_port ($) {
  my $port = shift;
  mtr_verbose("mtr_ping_port: $port");

  if (IS_WINDOWS && USE_NETPING) {
    # Under Windows, connect to a port that is not open is slow. It
    # takes ~1sec. Net::Ping with small timeout is much faster.
    my $ping = Net::Ping->new();
    $ping->port_number($port);

    if ($ping->ping("localhost", 0.1)) {
      mtr_verbose("USED");
      return 1;
    } else {
      mtr_verbose("FREE");
      return 0;
    }
  }

  my $remote = "localhost";
  my $iaddr  = inet_aton($remote);
  if (!$iaddr) {
    mtr_error("can't find IP number for $remote");
  }

  my $paddr = sockaddr_in($port, $iaddr);
  my $proto = getprotobyname('tcp');
  if (!socket(SOCK, PF_INET, SOCK_STREAM, $proto)) {
    mtr_error("can't create socket: $!");
  }

  if (connect(SOCK, $paddr)) {
    close(SOCK);    # FIXME check error?
    mtr_verbose("USED");
    return 1;
  } else {
    mtr_verbose("FREE");
    return 0;
  }
}

## Wait for the PID file to be created.
## FIXME check that the pidfile contains the expected pid!
sub sleep_until_pid_file_created ($$$) {
  my $pidfile = shift;
  my $timeout = shift;
  my $proc    = shift;

  my $sleeptime  = 100;                              # Milliseconds
  my $total_time = 0;                                # Milliseconds
  my $loops      = ($timeout * 1000) / $sleeptime;

  for (my $loop = 1 ; $loop <= $loops ; $loop++) {
    if (-r $pidfile) {
      mtr_verbose("Waited $total_time milliseconds for '$pidfile' file to " .
                  "be created.");
      return 1;
    }

    my $seconds = ($loop * $sleeptime) / 1000;

    # Check if it died after the fork() was successful
    if (defined $proc and !$proc->wait_one(0)) {
      mtr_warning("Process $proc died after mysql-test-run waited $seconds " .
                  "seconds for $pidfile to be created.");
      return 0;
    }

    $total_time = $total_time + 100;

    # 60 seconds wait between each message
    my $message_timeout = 600;
    if ($ENV{'VALGRIND_TEST'}) {
      $message_timeout = $message_timeout * 10;
    }

    # Print extra message every $message_timeout seconds
    if ($seconds > 1 &&
        int($seconds * 10) % $message_timeout == 0 &&
        $seconds < $timeout) {
      my $left = $timeout - $seconds;
      mtr_warning("Waited $seconds seconds for $pidfile to be created, " .
                  "still waiting for $left seconds...");
    }

    mtr_milli_sleep($sleeptime);
  }

  mtr_warning("Timeout after mysql-test-run waited $timeout seconds " .
              "for the process $proc to create a pid file.");
  return 0;
}

1;
