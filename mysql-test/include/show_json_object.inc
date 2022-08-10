# Utility function to extract only a certain
# JSON object/array from some optimizer trace.
# @param extract  a string of the form 'XXX S'
# where XXX is object or array's key, and S is the symbol after this
# key ( [ or { ).

--disable_query_log
DELIMITER |;
CREATE FUNCTION show_json_object(extract TEXT, trace LONGTEXT)
RETURNS TEXT
BEGIN
 DECLARE pos INT;
 DECLARE opened INT;
 DECLARE closed INT;
 DECLARE inside INT;
 DECLARE stop INT;
 DECLARE open_symbol VARCHAR(1);
 DECLARE close_symbol VARCHAR(1);
 SET pos = LOCATE(extract, trace);
 IF pos = 0
 THEN
   RETURN 'NOT FOUND';
 END IF;
 SET open_symbol = SUBSTRING(extract, -1);
 IF open_symbol = '{'
 THEN
   SET close_symbol = '}';
 ELSEIF open_symbol = '['
 THEN
   SET close_symbol = ']';
 ELSE
   RETURN 'ERROR: 1st argument does not end with { or [';
 END IF;
 SET trace = SUBSTRING(trace, pos);
 SET pos = 1;
 SET inside = 0;
 # A counter to break an infinite loop, in case of bug in SP or bad trace
 SET stop = 1000;
 LOOP
   SET stop = stop - 1;
   IF stop = 0
   THEN
     RETURN "ERROR";
   END IF;
   SET closed = LOCATE(close_symbol, trace, pos);
   SET opened = LOCATE(open_symbol, trace, pos);
   IF opened = 0
   THEN
     # symbol not found, treat it as "very far ahead"
     SET opened = 2000000;
   END IF;
   IF opened < closed
   THEN
     SET inside = inside + 1;
     SET pos = opened + 1;
   ELSE
     SET inside = inside - 1;
     SET pos = closed + 1;
   END IF;
   IF inside = 0
   THEN
     RETURN SUBSTRING(trace, 1, pos);
   END IF;
 END LOOP;
END|
DELIMITER ;|
--enable_query_log
