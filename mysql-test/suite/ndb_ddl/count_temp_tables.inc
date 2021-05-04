#
# Print number of tables with temp name present in NDB.
#
# This needs to be run after a COPY ALTER to verify that the
# tables with temp names don't exist in NDB.
#


--disable_query_log ONCE
# Print the number of tables with temp name
SELECT COUNT(*) AS 'Number of tables with temp name in NDB'
  FROM ndbinfo.dict_obj_info
  WHERE type = 2 /* User table */ AND
    SUBSTRING_INDEX(fq_name, '/', -1) LIKE "#sql%";
