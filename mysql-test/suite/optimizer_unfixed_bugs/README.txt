Putting testcases here
~~~~~~~~~~~~~~~~~~~~~~

When you work on re-verifying or re-classifying a bug (not fixing it),
it's a good idea to put the mtr-parsable '.test' testcase here.
Benefits:
1) tests downloaded from the bugs db are sometimes close to
mtr-parsable but not completely (for example if they contain
/* comment */); when you re-verify or re-classify you run the test so
may have to make it mtr-parsable; if you then put it in this suite,
the developer who will work on this bug in a few weeks or months will
not have to re-do the same download&fix, she/he can instead reuse your
work.
2) Others can see how their own work influences many unsolved
bugs, by running this suite. If they find that they fix a bug in this
suite, we won't later wonder "how come this bug doesn't happen
anymore, what fixed it?".
3) One can also run this suite with certain switches to see how they
influence unsolved bugs:
./mtr --suite=optimizer_unfixed_bugs \
--mysqld=--optimizer_switch="firstmatch=off"

Adding tests to this suite
~~~~~~~~~~~~~~~~~~~~~~~~~~
One test file per bug, named bugNNNNN.test.
Put the correct (not current and buggy) result file in r/, so that "[ pass ]"
in mtr will mean that a bug looks like fixed.
When you have fixed a bug, remove files from this suite.
t/bug45219.test is an example.
