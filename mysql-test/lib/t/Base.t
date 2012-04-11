# -*- cperl -*-

# Copyright (c) 2007 MySQL AB
# Use is subject to license terms.
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
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA

use Test::More qw(no_plan);
use strict;

use_ok ("My::SafeProcess::Base");


my $count= 0;
for (1..100){
  my $pid=  My::SafeProcess::Base::_safe_fork();
  exit unless $pid;
  (waitpid($pid, 0) == $pid) and $count++;
}
ok($count == 100, "safe_fork");

# A nice little forkbomb
SKIP: {
  skip("forkbomb", 1);
  eval {
    while(1){
      my $pid=  My::SafeProcess::Base::_safe_fork();
      exit unless $pid;
    }
  };
  ok($@, "forkbomb");
}

