# Setup things for handler.inc
#
# Input variables
#     $engine_type       -- storage engine to be tested
#     using btree          -- set if you want a non standard key type
#
# This scripts sets up default values for:
#     $other_handler_engine_type -- storage engine <> $engine_type, if possible
#                           1. $other_handler_engine_type must support handler
#                           2. $other_handler_engine_type must point to an all
#                              time available storage engine
# have to be set before sourcing this script.
#
# Handler tests don't work with embedded server
#
-- source include/not_embedded.inc

eval SET SESSION STORAGE_ENGINE = $engine_type;
let $other_handler_engine_type= MyISAM;

--disable_warnings
drop table if exists t1,t3,t4,t5;
--enable_warnings

# Create default test table

create table t1 (a int, b char(10), key a using btree (a), key b using btree (a,b));
insert into t1 values
(17,"ddd"),(18,"eee"),(19,"fff"),(19,"yyy"),
(14,"aaa"),(16,"ccc"),(16,"xxx"),
(20,"ggg"),(21,"hhh"),(22,"iii"),(23,"xxx"),(24,"xxx"),(25,"xxx");
