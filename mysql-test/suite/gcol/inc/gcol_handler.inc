################################################################################
# inc/gcol_handler.inc                                                         #
#                                                                              #
# Purpose:                                                                     #
#  Testing HANDLER.                                                            #
#                                                                              #
#                                                                              #
#                                                                              #
#------------------------------------------------------------------------------#
# Original Author: Andrey Zhakov                                               #
# Original Date: 2008-09-04                                                    #
# Change Author:                                                               #
# Change Date:                                                                 #
# Change:                                                                      #
################################################################################

create table t1 (a int, 
                 b int generated always as (-a) virtual,
                 c int generated always as (-a) stored,
		 d char(1),
		 index (a),
		 index (c));
insert into t1 (a,d) values (4,'a'), (2,'b'), (1,'c'), (3,'d');
select * from t1;

--echo # HANDLER tbl_name OPEN
handler t1 open;

--echo # HANDLER tbl_name READ non-gcol_index_name > (value1,value2,...)
handler t1 read a > (2);

--echo # HANDLER tbl_name READ non-gcol_index_name > (value1,value2,...) WHERE non-gcol_field=expr
handler t1 read a > (2) where d='c';

--echo # HANDLER tbl_name READ gcol_index_name = (value1,value2,...)
handler t1 read c = (-2);

--echo # HANDLER tbl_name READ gcol_index_name = (value1,value2,...) WHERE non-gcol_field=expr
handler t1 read c = (-2) where d='c';

--echo # HANDLER tbl_name READ non-gcol_index_name > (value1,value2,...) WHERE gcol_field=expr
handler t1 read a > (2) where b=-3 && c=-3;

--echo # HANDLER tbl_name READ gcol_index_name <= (value1,value2,...)
handler t1 read c <= (-2);

--echo # HANDLER tbl_name READ gcol_index_name > (value1,value2,...) WHERE gcol_field=expr
handler t1 read c <= (-2) where b=-3;

--echo # HANDLER tbl_name READ gcol_index_name FIRST
handler t1 read c first;

--echo # HANDLER tbl_name READ gcol_index_name NEXT
--skip_if_hypergraph
handler t1 read c next;

--echo # HANDLER tbl_name READ gcol_index_name PREV
--skip_if_hypergraph
handler t1 read c prev;

--echo # HANDLER tbl_name READ gcol_index_name LAST
--skip_if_hypergraph
handler t1 read c last;

--echo # HANDLER tbl_name READ FIRST where non-gcol=expr
handler t1 read FIRST where a >= 2; 

--echo # HANDLER tbl_name READ FIRST where gcol=expr
handler t1 read FIRST where b >= -2; 

--echo # HANDLER tbl_name READ NEXT where non-gcol=expr
handler t1 read NEXT where d='c'; 

--echo # HANDLER tbl_name READ NEXT where gcol=expr
handler t1 read NEXT where b<=-4; 

--echo # HANDLER tbl_name CLOSE
handler t1 close;

drop table t1;
