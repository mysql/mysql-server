# -*- cperl -*-
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

