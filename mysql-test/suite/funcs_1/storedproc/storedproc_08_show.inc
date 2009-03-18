#### suite/funcs_1/storedproc/storedproc_08_show.inc
#
#  used from .../storedproc_08.inc to show all created / altered routines

--echo
--echo ... now check what is stored:
--echo -----------------------------

--vertical_results

--replace_column 16 <modified> 17 <created>
SELECT * FROM information_schema.routines where routine_schema = 'db_storedproc';


SHOW CREATE FUNCTION  fn_1;
SHOW CREATE FUNCTION  fn_2;
#SHOW CREATE FUNCTION  fn_2b;

SHOW CREATE PROCEDURE sp_1;
SHOW CREATE PROCEDURE sp_2;


--replace_column 5 <modified> 6 <created>
SHOW FUNCTION STATUS LIKE 'fn_%';

--replace_column 5 <modified> 6 <created>
SHOW PROCEDURE STATUS LIKE 'sp_%';

--horizontal_results
