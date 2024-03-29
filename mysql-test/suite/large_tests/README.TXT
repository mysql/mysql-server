large_test suite
================

Iggy Galarza 07/20/07
---------------------

This test suite should include any test that requires:
1. Longer than the default timeout to complete.
2. A considerable amount of disk space.

These tests can be completed with the following command:

mysql-test-run.pl --suite=large_tests --big-test --suite-timeout=6360 --testcase-timeout=795
