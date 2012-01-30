# ==== Purpose ====
#
# Define the auxiliary stored functions operating on sets of GTIDs.
# The functions are typically useful to check assertions related to
# GTIDs. The following functions are defined:
#
#   GTID_IS_EQUAL(g1, g2)
#     True if g1 and g2 are the same set
#   GTID_IS_DISJOINT(g1, g2)
#     True if g1 and g2 are disjoint
#   GTID_IS_DISJOINT_UNION(g1, g2, sum)
#     True if sum is the disjoint union of g1 and g2
#   GTID_UNION(g1, g2)
#     Return the union of g1 and g2
#   GTID_INTERSECTION(g1, g2)
#     Return the union of g1 and g2
#   GTID_SYMMETRIC_DIFFERENCE(g1, g2)
#     Return the symmetric difference of g1 and g2
#   GTID_SUBTRACT_UUID(gtid, uuid)
#     Remove the UUID from the GTID set
#   GTID_INTERSECTION_WITH_UUID(gtid, uuid)
#     Return only the gtids with the given uuid from the GTID set
#   GTID_COMPARE(old, diff, new)
#     The same as GTID_IS_DISJOINT_UNION, except it has the following
#     additional feature: if diff begins by '~', then it returns true
#     if 'old' is the disjoint union of 'new' and 'diff' (where the
#     tilde is removed).
#   NUMBER_TO_UUID(N)
#     Given a number N between 0 and 99, returns a uuid consisting of the
#     number repeated, like:
#       NUMBER_TO_UUID(1) -> '01010101-0101-0101-0101-010101010101'
#     This works also if N is a GTID set, like:
#       NUMBER_TO_UUID('1:4-5,97:1') ->
#         '01010101-0101-0101-0101-010101010101:4-5,
#          97979797-9797-9797-9797-979797979797:1'
#   UUID_TO_NUMBER(UUID)
#     The inverse of NUMBER_TO_UUID.
#
# ==== Usage ====
#
#  1. Create the stored functions at the beginning of the test script:
#
#       --source include/gtid_utils.inc
# 
#  2. Use the functions as you like during the test script.
#
#  3. Delete the stored functions at the end of the test script:
#
#       --source include/gtid_utils_end.inc


--let $include_filename= gtid_utils.inc
--source include/begin_include_file.inc

--disable_query_log

--delimiter |

# For convenience, declare these human-readable, deterministic uuids here
--let $uuid0= 00000000-0000-0000-0000-000000000000
--let $uuid1= 11111111-1111-1111-1111-111111111111
--let $uuid2= 22222222-2222-2222-2222-222222222222
--let $uuid3= 33333333-3333-3333-3333-333333333333
--let $uuid4= 44444444-4444-4444-4444-444444444444
--let $uuid5= 55555555-5555-5555-5555-555555555555
--let $uuid6= 66666666-6666-6666-6666-666666666666
--let $uuid7= 77777777-7777-7777-7777-777777777777
--let $uuid8= 88888888-8888-8888-8888-888888888888
--let $uuid9= 99999999-9999-9999-9999-999999999999
--let $uuida= aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa
--let $uuidb= bbbbbbbb-bbbb-bbbb-bbbb-bbbbbbbbbbbb
--let $uuidc= cccccccc-cccc-cccc-cccc-cccccccccccc
--let $uuidd= dddddddd-dddd-dddd-dddd-dddddddddddd
--let $uuide= eeeeeeee-eeee-eeee-eeee-eeeeeeeeeeee
--let $uuidf= ffffffff-ffff-ffff-ffff-ffffffffffff

# Return nonzero if the two gtid_sets are equal.
CREATE FUNCTION GTID_IS_EQUAL(g1 TEXT(10000), g2 TEXT(10000))
RETURNS INT
BEGIN
  RETURN GTID_SUBSET(g1, g2) AND GTID_SUBSET(g2, g1);
END|

# Return nonzero if the two gtid_sets are disjoint.
CREATE FUNCTION GTID_IS_DISJOINT(g1 TEXT(10000), g2 TEXT(10000))
RETURNS INT
BEGIN
  RETURN GTID_SUBSET(g1, GTID_SUBTRACT(g1, g2));
END|

# Return true if g1 union g2 = sum, and g1 and g2 are disjoint.
CREATE FUNCTION GTID_IS_DISJOINT_UNION(g1 TEXT(10000), g2 TEXT(10000), sum TEXT(10000))
RETURNS INT
BEGIN
  RETURN GTID_IS_EQUAL(GTID_SUBTRACT(sum, g1), g2) AND
         GTID_IS_EQUAL(GTID_SUBTRACT(sum, g2), g1);
END|

# Return the union of g1 and g2.
CREATE FUNCTION GTID_UNION(g1 TEXT(10000), g2 TEXT(10000))
RETURNS TEXT(10000)
BEGIN
  RETURN GTID_SUBTRACT(CONCAT(g1, ',', g2), '');
END|

# Return the intersection of g1 and g2.
CREATE FUNCTION GTID_INTERSECT(g1 TEXT(10000), g2 TEXT(10000))
RETURNS TEXT(10000)
BEGIN
  RETURN GTID_SUBTRACT(g1, GTID_SUBTRACT(g1, g2));
END|

# Return the symmetric difference between g1 and g2.
CREATE FUNCTION GTID_SYMMETRIC_DIFFERENCE(g1 TEXT(10000), g2 TEXT(10000))
RETURNS TEXT(10000)
BEGIN
  RETURN SUBTRACT(CONCAT(g1, ',', g2), GTID_INTERSECTION(g1, g2));
END|

# Return the gtid with uuid removed.
CREATE FUNCTION GTID_SUBTRACT_UUID(gtid TEXT(10000), uuid TEXT(10000))
RETURNS TEXT(10000)
BEGIN
  RETURN GTID_SUBTRACT(gtid, CONCAT(UUID, ':1-', 1 << 62));
END|

# Return the intersection of gtid and uuid.
CREATE FUNCTION GTID_INTERSECT_UUID(gtid TEXT(10000), uuid TEXT(10000))
RETURNS TEXT(10000)
BEGIN
  RETURN GTID_SUBTRACT(gtid, GTID_SUBTRACT_UUID(gtid, uuid));
END|

# Return true if gtid_set 'new' is obtained by adding 'diff' to 'old',
# and all three are disjoint.  If 'diff' is prefixed by a tilde, then
# returns true if 'old' is obtained by adding 'diff' (with the tilde
# removed) from 'new'.
CREATE FUNCTION GTID_COMPARE(old TEXT(10000), diff TEXT(10000), new TEXT(10000))
RETURNS INT
BEGIN
  DECLARE tilde_pos INT DEFAULT LOCATE('~', diff);
  DECLARE to_add TEXT(10000);
  DECLARE to_remove TEXT(10000);
  IF tilde_pos != 0 THEN
    RETURN GTID_IS_DISJOINT_UNION(new, SUBSTR(diff, 2), old);
  ELSE
    RETURN GTID_IS_DISJOINT_UNION(old, diff, new);
  END IF;
END|

# convert strings like '1:4-5,97:1' to
# '01010101-0101-0101-0101-010101010101:4-5,97979797-9797-9797-9797-979797979797:1'
CREATE FUNCTION NUMBER_TO_UUID(str TEXT(10000))
RETURNS TEXT(10000)
BEGIN
  DECLARE pos INT DEFAULT 0;# pos. before next number to replace by UUID in str
  DECLARE colon INT;               # position of next ':' after pos in str
  DECLARE n TEXT(3);               # number between comma and colon
  DECLARE nn TEXT(5);              # n, zero-padded to 2 digits, repeated twice
  DECLARE _uuid TEXT(37);          # UUID generated from nn
  DECLARE comma INT;               # position of next ',' after pos
  DECLARE tilde INT;               # position of next '~' after pos
  IF str = '' or str IS NULL THEN
    RETURN str;
  END IF;
  IF SUBSTR(str, 1, 1) = '~' THEN
    SET pos = 1;
  END IF;
  REPEAT
    # find end of number
    SET colon = LOCATE(':', str, pos + 1);
    # get number
    SET n = SUBSTR(str, pos + 1, colon - pos - 1);
    # convert number to uuid
    SET nn = REPEAT(LPAD(n, 2, '0'), 2);
    SET _uuid = CONCAT(nn, nn, '-', nn, '-', nn, '-', nn, '-', nn, nn, nn);
    # replace number by uuid
    SET str = CONCAT(SUBSTR(str, 1, pos), _uuid, SUBSTR(str, colon));
    # find next substring to replace
    SET comma = LOCATE(',', str, pos + 1), tilde = LOCATE('~', str, pos + 1);
    SET pos = IF(comma != 0 AND (tilde = 0 OR comma < tilde), comma, tilde);
  UNTIL pos = 0 END REPEAT;
  RETURN str;
END|

# convert strings like '01010101-0101-0101-0101-010101010101:4-5,
# 97979797-9797-9797-9797-979797979797:1'
# to '1:4-5,97:1'
CREATE FUNCTION UUID_TO_NUMBER(str TEXT(10000))
RETURNS TEXT(10000)
BEGIN
  DECLARE pos INT DEFAULT 0;       # position before next UUID in str
  DECLARE colon INT;               # position of first ':' after pos in str
  DECLARE _uuid TEXT(37);          # UUID between ',' and ':'
  DECLARE n TEXT(5);               # the two digits that are repeated in UUID
  DECLARE comma INT;               # position of next ',' after pos
  DECLARE tilde INT;               # position of next '~' after pos
  IF str = '' or str IS NULL THEN
    RETURN str;
  END IF;
  IF SUBSTR(str, 1, 1) = '~' THEN
    SET pos = 1;
  END IF;
  # trim newlines
  SET str = REPLACE(str, '\n', '');
  REPEAT
    # find end of substring of the form ",uuid:"
    SET colon = LOCATE(':', str, pos + 1);
    # get the uuid
    SET _uuid = SUBSTR(str, pos + 1, colon - pos - 1);
    IF _uuid = @@GLOBAL.SERVER_UUID THEN
      # strip server_uuid from string. make sure we dont strip ~
      # before or after server_uuid.
      SET comma = LOCATE(',', str, pos + 1), tilde = LOCATE('~', str, pos + 1);
      IF comma != 0 AND (tilde = 0 OR tilde > comma) THEN
        SET str = CONCAT(SUBSTR(str, 1, pos), SUBSTR(str, comma + 1));
      ELSEIF tilde != 0 THEN
        SET str = CONCAT(SUBSTR(str, 1, pos - 1), SUBSTR(str, tilde));
      ELSE
        SET str = SUBSTR(str, 1, pos - 1);
      END IF;
    ELSE
      # get the number that repeats in UUID and remove leading zero
      SET n = SUBSTR(str, pos + 1, 2);
      IF SUBSTR(n, 1, 1) = '0' THEN
        SET n = SUBSTR(n, 2, 1);
      END IF;
      # replace UUID by number
      SET str = CONCAT(SUBSTR(str, 1, pos), n, SUBSTR(str, colon));
    END IF;
    # find next substring
    SET comma = LOCATE(',', str, pos + 1), tilde = LOCATE('~', str, pos + 1);
    SET pos = IF(comma != 0 AND (tilde = 0 OR comma < tilde), comma, tilde);
  UNTIL pos = 0 END REPEAT;
  RETURN str;
END|

--delimiter ;

--let $include_filename= gtid_utils.inc
--source include/end_include_file.inc
