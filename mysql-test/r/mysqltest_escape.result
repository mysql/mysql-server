==== R1: escape inserts backslash before character ====
<'"\\f\\o\\o"'>
<'\"\\f\\o\\o\"'>
<\'\"\\f\\o\\o\"\'>
==== R2: escape preserves leading and trailing whitespace ====
< h\ah\a >
<\ \ \ >
==== R3: escape(C,S) shall work if S is empty  ====
<>
==== R4.1: escape(C,S) shall work when C or S contains parentheses ====
< \hi\hi (\ha\ha)>
< hihi (haha\)>
< hihi \(haha)>
==== R4.2: escape(C,S) shall work when C or S contains comma ====
<,x,>
<\,x\,>
==== R4.3: escape(C,S) shall work when S contains newline ====
<


\ho\ho
\hu\hu

>
==== R5: escape shall work with multiple characters ====
<He\l\l\o, w\or\ld>
<Hell\o\, w\orld>
==== R6: escape(C,S) shall fail when C is newline ====
mysqltest: At line 1: Invalid format of 'escape' arguments: <(
,
foo
)>
==== R7.1: escape shall fail when opening parenthesis is wrong ====
mysqltest: At line 1: Invalid format of 'escape' arguments: < (a,aa)>
mysqltest: At line 1: Invalid format of 'escape' arguments: < a,aa)>
==== R7.2: escape shall fail when comma is wrong ====
mysqltest: At line 1: Invalid format of 'escape' arguments: <(aaa)>
mysqltest: At line 1: Invalid format of 'escape' arguments: <(,aaa)>
mysqltest: At line 1: Invalid format of 'escape' arguments: <(,)>
==== R7.3: escape shall fail when closing parenthesis is wrong ====
mysqltest: At line 1: Invalid format of 'escape' arguments: <(a,aa>
mysqltest: At line 1: Invalid format of 'escape' arguments: <(a,aa)a>
==== R7.4: escape shall fail when parentheses and comma are out of order ====
mysqltest: At line 1: Invalid format of 'escape' arguments: <,()>
mysqltest: At line 1: Invalid format of 'escape' arguments: <,)(>
mysqltest: At line 1: Invalid format of 'escape' arguments: <),(>
mysqltest: At line 1: Invalid format of 'escape' arguments: <(),>
mysqltest: At line 1: Invalid format of 'escape' arguments: <)(,>
