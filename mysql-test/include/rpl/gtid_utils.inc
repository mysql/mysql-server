# ==== Purpose ====
#
# Define the auxiliary stored functions operating on sets of GTIDs.
# The functions are typically useful to check assertions related to
# GTIDs. The following functions are defined:
#
#   GTID_IS_EQUAL(gtid_set_1, gtid_set_2)
#     True if gtid_set_1 and gtid_set_2 are the same set
#   GTID_IS_DISJOINT(gtid_set_1, gtid_set_2)
#     True if gtid_set_1 and gtid_set_2 are disjoint
#   GTID_IS_DISJOINT_UNION(gtid_set_1, gtid_set_2, sum)
#     True if sum is the disjoint union of gtid_set_1 and gtid_set_2
#   GTID_NORMALIZE(gtid_set)
#     Return the gtid set in a normalized form: all uuids in alphabetic
#     order, all intervals compressed and in order, no whitespace, all
#     lowercase.
#   GTID_UNION(gtid_set_1, gtid_set_2)
#     Return the union of gtid_set_1 and gtid_set_2
#   GTID_INTERSECTION(gtid_set_1, gtid_set_2)
#     Return the intersection of gtid_set_1 and gtid_set_2
#   GTID_SYMMETRIC_DIFFERENCE(gtid_set_1, gtid_set_2)
#     Return the symmetric difference of gtid_set_1 and gtid_set_2
#   GTID_SUBTRACT_UUID(gtid_set, uuid)
#     Remove the UUID from the GTID set
#   GTID_INTERSECTION_WITH_UUID(gtid_set, uuid)
#     Return only the gtids with the given uuid from the GTID set
#   GTID_COUNT(gtid_set)
#     Return the number of gtids in the gtid_set.
#   GTID_FROM_GTID_SET(gtid_set)
#     Return one GTID from the given GTID set.
#   GTID_NEXT_GENERATED(gtid_set, uuid)
#     Return the next GNO that will be generated for the given uuid.
#   GTID_NEXT_GENERATED_MULTIPLE(gtid_set, uuid, count)
#     Return the GTID set consisting of the next 'count' GTIDs that will
#     be generated for the given uuid.
#   GTID_NEXT_GENERATED_SET(base, diff)
#     Return the GTID set obtained by replacing any substring of diff
#     that has the form 'UUID:+N' by
#     GTID_NEXT_GENERATED_MULTIPLE(base, UUID, N).
#   GTID_COMPARE(old, diff, new)
#     The same as GTID_IS_DISJOINT_UNION, except it has the following
#     additional feature: if diff begins by '~', then it returns true
#     if 'old' is the disjoint union of 'new' and 'diff' (where the
#     tilde is removed).
#   GTID_EXECUTED_FROM_TABLE()
#     Return a canonicalized Gtid_set containing the GTIDs in the
#     mysql.gtid_executed table.
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
# You should not normally source this file directly; instead set
# $rpl_gtid_utils=1 before sourcing rpl/init_source_replica.inc or rpl/init.inc.
# Then, the functions are automatically dropped in rpl/deinit.inc, and
# RESET BINARY LOGS AND GTIDS is executed so that the functions do not pollute the
# binary log.
#
# If you really need to use this file manually, do this:
#
#  1. Create the stored functions at the beginning of the test script:
#
#       --source include/rpl/gtid_utils.inc
#
#  2. Use the functions as you like during the test script.
#
#  3. Delete the stored functions at the end of the test script:
#
#       --source include/rpl/gtid_utils_end.inc


--let $include_filename= rpl/gtid_utils.inc
--source include/begin_include_file.inc

if (!$rpl_debug)
{
  --disable_query_log
}
--let $gtid_utils_included=1

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
CREATE FUNCTION GTID_IS_EQUAL(gtid_set_1 LONGTEXT, gtid_set_2 LONGTEXT)
RETURNS INT
RETURN GTID_SUBSET(gtid_set_1, gtid_set_2) AND GTID_SUBSET(gtid_set_2, gtid_set_1)|

# Return nonzero if the two gtid_sets are disjoint.
CREATE FUNCTION GTID_IS_DISJOINT(gtid_set_1 LONGTEXT, gtid_set_2 LONGTEXT)
RETURNS INT
RETURN GTID_SUBSET(gtid_set_1, GTID_SUBTRACT(gtid_set_1, gtid_set_2))|

# Return true if gtid_set_1 union gtid_set_2 = sum, and gtid_set_1 and gtid_set_2 are disjoint.
CREATE FUNCTION GTID_IS_DISJOINT_UNION(gtid_set_1 LONGTEXT, gtid_set_2 LONGTEXT, sum LONGTEXT)
RETURNS INT
RETURN GTID_IS_EQUAL(GTID_SUBTRACT(sum, gtid_set_1), gtid_set_2) AND
       GTID_IS_EQUAL(GTID_SUBTRACT(sum, gtid_set_2), gtid_set_1)|

# Return a normalized form of the GTID (all uppercase, no whitespace,
# no duplicates, uuids in alphabetic order, intervals in numeric
# order).
CREATE FUNCTION GTID_NORMALIZE(g LONGTEXT)
RETURNS LONGTEXT
RETURN GTID_SUBTRACT(g, '')|

# Return the union of gtid_set_1 and gtid_set_2.
CREATE FUNCTION GTID_UNION(gtid_set_1 LONGTEXT, gtid_set_2 LONGTEXT)
RETURNS LONGTEXT
RETURN GTID_NORMALIZE(CONCAT(gtid_set_1, ',', gtid_set_2))|

# Return the intersection of gtid_set_1 and gtid_set_2.
CREATE FUNCTION GTID_INTERSECTION(gtid_set_1 LONGTEXT, gtid_set_2 LONGTEXT)
RETURNS LONGTEXT
RETURN GTID_SUBTRACT(gtid_set_1, GTID_SUBTRACT(gtid_set_1, gtid_set_2))|

# Return the symmetric difference between gtid_set_1 and gtid_set_2.
CREATE FUNCTION GTID_SYMMETRIC_DIFFERENCE(gtid_set_1 LONGTEXT, gtid_set_2 LONGTEXT)
RETURNS LONGTEXT
RETURN GTID_SUBTRACT(CONCAT(gtid_set_1, ',', gtid_set_2), GTID_INTERSECTION(gtid_set_1, gtid_set_2))|

# Return the gtid with uuid removed.
CREATE FUNCTION GTID_SUBTRACT_UUID(gtid_set LONGTEXT, uuid TEXT)
RETURNS LONGTEXT
RETURN GTID_SUBTRACT(gtid_set, CONCAT(UUID, ':1-', (1 << 63) - 2))|

# Return the intersection of gtid and uuid.
CREATE FUNCTION GTID_INTERSECTION_WITH_UUID(gtid_set LONGTEXT, uuid TEXT)
RETURNS LONGTEXT
RETURN GTID_SUBTRACT(gtid_set, GTID_SUBTRACT_UUID(gtid_set, uuid))|

# If the first argument is nonzero, return it, else return the second argument
CREATE FUNCTION IFZERO(a INT, b INT)
RETURNS INT
RETURN IF(a = 0, b, a)|

# Like the builtin LOCATE, but returns length+1 rather than 0 if nothing found.
CREATE FUNCTION LOCATE2(needle LONGTEXT, haystack LONGTEXT, offset INT)
RETURNS INT
RETURN IFZERO(LOCATE(needle, haystack, offset), LENGTH(haystack) + 1)|

# Return the number of GTIDs in the given GTID set.
CREATE FUNCTION GTID_COUNT(gtid_set LONGTEXT)
RETURNS INT
BEGIN
  DECLARE result BIGINT DEFAULT 0;
  DECLARE colon_pos INT;
  DECLARE next_dash_pos INT;
  DECLARE next_colon_pos INT;
  DECLARE next_comma_pos INT;
  DECLARE is_tag INT;
  SET gtid_set = GTID_NORMALIZE(gtid_set);
  SET colon_pos = LOCATE2(':', gtid_set, 1);
  WHILE colon_pos != LENGTH(gtid_set) + 1 DO
     SET next_dash_pos = LOCATE2('-', gtid_set, colon_pos + 1);
     SET next_colon_pos = LOCATE2(':', gtid_set, colon_pos + 1);
     SET next_comma_pos = LOCATE2(',', gtid_set, colon_pos + 1);
     SET is_tag = REGEXP_LIKE(SUBSTR(gtid_set, colon_pos + 1, 1), '^[a-zA-Z_]');
     IF next_dash_pos < next_colon_pos AND next_dash_pos < next_comma_pos AND is_tag = 0 THEN
       SET result = result +
         SUBSTR(gtid_set, next_dash_pos + 1,
                LEAST(next_colon_pos, next_comma_pos) - (next_dash_pos + 1)) -
         SUBSTR(gtid_set, colon_pos + 1, next_dash_pos - (colon_pos + 1)) + 1;
     ELSEIF is_tag = 0 THEN
       SET result = result + 1;
     END IF;
     SET colon_pos = next_colon_pos;
  END WHILE;
  RETURN result;
END|

# Returns one GTID from a GTID set.
CREATE FUNCTION GTID_FROM_GTID_SET(gtid_set LONGTEXT)
RETURNS LONGTEXT
BEGIN
  DECLARE normalized LONGTEXT DEFAULT GTID_NORMALIZE(gtid_set);
  DECLARE end_of_number INT
    DEFAULT LEAST(LOCATE2('-', normalized, 38),
                  LOCATE2(':', normalized, 38),
                  LOCATE2(',', normalized, 38));
  RETURN SUBSTR(normalized, 1, end_of_number - 1);
END|

# Return the next GNO (numeric component) to be generated for the given UUID
CREATE FUNCTION GTID_NEXT_GENERATED(gtid_set LONGTEXT, uuid TEXT)
RETURNS INT
BEGIN
  DECLARE gtid_uuid LONGTEXT DEFAULT GTID_INTERSECTION_WITH_UUID(gtid_set, uuid);
  DECLARE colon_pos INT DEFAULT LOCATE2(':', gtid_uuid, 1);
  DECLARE next_dash_pos INT DEFAULT LOCATE2('-', gtid_uuid, colon_pos + 1);
  DECLARE next_comma_pos INT DEFAULT LOCATE2(',', gtid_uuid, colon_pos + 1);
  DECLARE next_colon_pos INT DEFAULT LOCATE2(':', gtid_uuid, colon_pos + 1);
  IF gtid_uuid = '' THEN
    RETURN 1;
  ELSEIF SUBSTR(gtid_uuid, colon_pos + 1,
                 LEAST(next_dash_pos, next_comma_pos, next_colon_pos) -
                 (colon_pos + 1)) != '1' THEN
    RETURN 1;
  ELSEIF next_dash_pos < LEAST(next_comma_pos, next_colon_pos) THEN
    RETURN SUBSTR(gtid_uuid, next_dash_pos + 1,
                  LEAST(next_comma_pos, next_colon_pos) -
                  (next_dash_pos + 1)) + 1;
  ELSE
    RETURN 2;
  END IF;
END|

# Return a GTID set consisting of the the next 'count' GTIDs that will
# be generated for the given UUID.
# This is inefficient if count is big, consider optimizing it if needed.
CREATE FUNCTION GTID_NEXT_GENERATED_MULTIPLE(gtid_set LONGTEXT, uuid TEXT, count INT)
RETURNS LONGTEXT
BEGIN
  DECLARE result LONGTEXT DEFAULT '';
  DECLARE number INT;
  DECLARE new_gtid LONGTEXT;
  WHILE count > 0 DO
    SET new_gtid = CONCAT(uuid, ':', GTID_NEXT_GENERATED(gtid_set, uuid));
    SET result = GTID_UNION(result, new_gtid);
    SET gtid_set = GTID_UNION(gtid_set, new_gtid);
    SET count = count - 1;
  END WHILE;
  RETURN result;
END|

# Return the GTID set obtained by replacing any substring of diff that
# has the form 'UUID:+N' by GTID_NEXT_GENERATED_MULTIPLE(base, UUID, N).
CREATE FUNCTION GTID_NEXT_GENERATED_SET(base LONGTEXT, diff LONGTEXT)
RETURNS LONGTEXT
BEGIN
  DECLARE colon_plus_pos INT DEFAULT LOCATE(':+', diff);
  DECLARE uuid TEXT;
  DECLARE end_of_number_pos INT;
  DECLARE number INT;
  WHILE colon_plus_pos != 0 DO
    SET uuid = SUBSTR(diff, colon_plus_pos - 36, 36);
    SET end_of_number_pos= LEAST(LOCATE2(':', diff, colon_plus_pos + 2), LOCATE2(',', diff, colon_plus_pos + 2));
    SET number = SUBSTR(diff, colon_plus_pos + 2, end_of_number_pos - colon_plus_pos - 2);
    SET diff = CONCAT(SUBSTR(diff, 1, colon_plus_pos - 1), SUBSTR(diff, end_of_number_pos), ',', GTID_NEXT_GENERATED_MULTIPLE(base, uuid, number));
    SET colon_plus_pos = LOCATE(':+', diff);
  END WHILE;
  RETURN GTID_NORMALIZE(diff);
END|

# Return true if gtid_set 'new' is obtained by adding 'diff' to 'old',
# and all three are disjoint.
#
# If 'diff' is prefixed by a tilde, then returns true if 'old' is
# obtained by adding 'diff' (with the tilde removed) from 'new'.
#
# If 'diff' contains substrings of the format UUID:+N, then this is
# interpreted as GTID_NEXT_GENERATED_SET(old, diff).
CREATE FUNCTION GTID_COMPARE(old LONGTEXT, diff LONGTEXT, new LONGTEXT)
RETURNS LONGTEXT
RETURN IF(SUBSTR(diff, 1, 1) != '~',
          GTID_IS_DISJOINT_UNION(old, GTID_NEXT_GENERATED_SET(old, diff), new),
          GTID_IS_DISJOINT_UNION(new, GTID_NEXT_GENERATED_SET(old, SUBSTR(diff, 2)), old))|

# Read a Gtid_set from gtid_executed table
CREATE FUNCTION GTID_EXECUTED_FROM_TABLE()
RETURNS LONGTEXT
BEGIN
  DECLARE old_group_concat_max_len INT DEFAULT @@SESSION.GROUP_CONCAT_MAX_LEN;
  DECLARE tmp LONGTEXT;
  SET @@SESSION.GROUP_CONCAT_MAX_LEN = 100000;
  SELECT GROUP_CONCAT(CONCAT(source_uuid, ':', interval_start, '-', interval_end) SEPARATOR ',') FROM mysql.gtid_executed INTO tmp;
  SET @@SESSION.GROUP_CONCAT_MAX_LEN = old_group_concat_max_len;
  RETURN GTID_NORMALIZE(tmp);
END|

# convert strings like '1:4-5,97:1' to
# '01010101-0101-0101-0101-010101010101:4-5,97979797-9797-9797-9797-979797979797:1'
CREATE FUNCTION NUMBER_TO_UUID(str LONGTEXT)
RETURNS LONGTEXT
BEGIN
  DECLARE pos INT DEFAULT 0;# pos. before next number to replace by UUID in str
  DECLARE colon INT;               # position of next ':' after pos in str
  DECLARE n TEXT;                  # number between comma and colon
  DECLARE nn TEXT;                 # n, zero-padded to 2 digits, repeated twice
  DECLARE _uuid TEXT;              # UUID generated from nn
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
    # length != 4 happens if the string already contains a uuid
    IF LENGTH(nn) = 4 THEN
      SET _uuid = CONCAT(nn, nn, '-', nn, '-', nn, '-', nn, '-', nn, nn, nn);
      # replace number by uuid
      SET str = CONCAT(SUBSTR(str, 1, pos), _uuid, SUBSTR(str, colon));
    END IF;
    # find next substring to replace
    SET comma = LOCATE(',', str, pos + 1), tilde = LOCATE('~', str, pos + 1);
    SET pos = IF(comma != 0 AND (tilde = 0 OR comma < tilde), comma, tilde);
  UNTIL pos = 0 END REPEAT;
  RETURN str;
END|

# convert strings like '01010101-0101-0101-0101-010101010101:4-5,
# 97979797-9797-9797-9797-979797979797:1'
# to '1:4-5,97:1'
CREATE FUNCTION UUID_TO_NUMBER(str LONGTEXT)
RETURNS LONGTEXT
BEGIN
  DECLARE pos INT DEFAULT 0;       # position before next UUID in str
  DECLARE colon INT;               # position of first ':' after pos in str
  DECLARE _uuid TEXT;              # UUID between ',' and ':'
  DECLARE n TEXT;                  # the two digits that are repeated in UUID
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

--let $include_filename= rpl/gtid_utils.inc
--source include/end_include_file.inc
