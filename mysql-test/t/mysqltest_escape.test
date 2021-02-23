# ==== Purpose ====
#
# Verify that the "escape" function in the mysqltest language works as expected.
#
# ==== Requirements ====
#
# R1. escape(C,S) shall insert a backslash character before every occurrence
#     of C in S and return the resulting string.
# R2. escape(C,S) shall preserve leading and trailing whitespace in S.
# R3. escape(C,S) shall work if S is empty.
# R4. escape(C,S) shall work when C or S contains parentheses or commas, and
#     when S contains newlines.
# R5. escape(C,S) shall work when C is multi-character.
# R6. escape(C,S) shall fail when C is newline.
# R7. escape shall fail when the parentheses or comma are wrong.

--echo ==== R1: escape inserts backslash before character ====

--let $x = foo bar baz
--let $y = escape(a,$x)
--assert($x == foo bar baz)
--assert($y == foo b\ar b\az)
--let $x = '"\f\o\o"'
--let $y = escape(\,$x)
--let $z = escape(",$y)
--let $w = escape(',$z)
--echo <$y>
--echo <$z>
--echo <$w>

--echo ==== R2: escape preserves leading and trailing whitespace ====

--let $x = escape(a, haha )
--echo <$x>

--let $s = `SELECT ' '`
--let $x = escape( , $s )
--echo <$x>

--echo ==== R3: escape(C,S) shall work if S is empty  ====

--let $x = escape(a,)
--echo <$x>

--echo ==== R4.1: escape(C,S) shall work when C or S contains parentheses ====

--let $x = escape(h, hihi (haha))
--echo <$x>
--let $x = escape(), hihi (haha))
--echo <$x>
--let $x = escape((, hihi (haha))
--echo <$x>

--echo ==== R4.2: escape(C,S) shall work when C or S contains comma ====

--let $x = escape(u,,x,)
--echo <$x>
--let $x = escape(,,,x,)
--echo <$x>

--echo ==== R4.3: escape(C,S) shall work when S contains newline ====

--let $nl = `SELECT '\n'`
let $x = escape(h,$nl$nl$sa
hoho
huhu$nl$nl);
--echo <$x>

--echo ==== R5: escape shall work with multiple characters ====

--let $x = escape(lo,Hello, world)
--echo <$x>

--let $x = escape(,o,Hello, world)
--echo <$x>

--echo ==== R6: escape(C,S) shall fail when C is newline ====

--let $test_file = $MYSQL_TMP_DIR/escape_failure.test
--write_file $test_file
let $x = escape(
,
foo
);
EOF
--error 1
--exec $MYSQL_TEST < $test_file 2>&1
--remove_file $test_file

--echo ==== R7.1: escape shall fail when opening parenthesis is wrong ====

--write_file $test_file
--let $x = escape (a,aa)
EOF
--error 1
--exec $MYSQL_TEST < $test_file 2>&1
--remove_file $test_file

--write_file $test_file
--let $x = escape a,aa)
EOF
--error 1
--exec $MYSQL_TEST < $test_file 2>&1
--remove_file $test_file

--echo ==== R7.2: escape shall fail when comma is wrong ====

--write_file $test_file
--let $x = escape(aaa)
EOF
--error 1
--exec $MYSQL_TEST < $test_file 2>&1
--remove_file $test_file

--write_file $test_file
--let $x = escape(,aaa)
EOF
--error 1
--exec $MYSQL_TEST < $test_file 2>&1
--remove_file $test_file

--write_file $test_file
--let $x = escape(,)
EOF
--error 1
--exec $MYSQL_TEST < $test_file 2>&1
--remove_file $test_file

--echo ==== R7.3: escape shall fail when closing parenthesis is wrong ====

--write_file $test_file
--let $x = escape(a,aa
EOF
--error 1
--exec $MYSQL_TEST < $test_file 2>&1
--remove_file $test_file

--write_file $test_file
--let $x = escape(a,aa)a
EOF
--error 1
--exec $MYSQL_TEST < $test_file 2>&1
--remove_file $test_file

--echo ==== R7.4: escape shall fail when parentheses and comma are out of order ====

--write_file $test_file
--let $x = escape,()
EOF
--error 1
--exec $MYSQL_TEST < $test_file 2>&1
--remove_file $test_file

--write_file $test_file
--let $x = escape,)(
EOF
--error 1
--exec $MYSQL_TEST < $test_file 2>&1
--remove_file $test_file

--write_file $test_file
--let $x = escape),(
EOF
--error 1
--exec $MYSQL_TEST < $test_file 2>&1
--remove_file $test_file

--write_file $test_file
--let $x = escape(),
EOF
--error 1
--exec $MYSQL_TEST < $test_file 2>&1
--remove_file $test_file

--write_file $test_file
--let $x = escape)(,
EOF
--error 1
--exec $MYSQL_TEST < $test_file 2>&1
--remove_file $test_file
