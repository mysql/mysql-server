# Create helper functions for comparing INNODB_CACHED_INDEXES with direct
# buffer-pool walks.
#
# get_n_cached_pages() is the tested value from
# INFORMATION_SCHEMA.INNODB_CACHED_INDEXES.N_CACHED_PAGES, which counts leaf
# pages only. count_index_pages_in_lru() scans
# INFORMATION_SCHEMA.INNODB_BUFFER_PAGE_LRU, which walks the actual LRU list and
# therefore includes compressed-only page descriptors too. It also includes
# non-leaf B-tree and R-tree index pages.
#
--delimiter |
CREATE FUNCTION get_n_cached_pages(table_name VARCHAR(512),
                                   index_name VARCHAR(64))
RETURNS BIGINT
READS SQL DATA
BEGIN
  DECLARE pages BIGINT DEFAULT 0;

  SELECT COALESCE(SUM(c.N_CACHED_PAGES), 0)
    INTO pages
  FROM INFORMATION_SCHEMA.INNODB_TABLES t
  JOIN INFORMATION_SCHEMA.INNODB_INDEXES i
    ON i.TABLE_ID = t.TABLE_ID
  LEFT JOIN INFORMATION_SCHEMA.INNODB_CACHED_INDEXES c
    ON c.SPACE_ID = i.SPACE
   AND c.INDEX_ID = i.INDEX_ID
  WHERE t.NAME = table_name
    AND i.NAME = index_name;

  RETURN pages;
END|

CREATE FUNCTION count_index_pages_in_lru(table_name VARCHAR(512),
                                         index_name VARCHAR(64))
RETURNS BIGINT
READS SQL DATA
BEGIN
  DECLARE pages BIGINT DEFAULT 0;

  SELECT COUNT(*)
    INTO pages
  FROM INFORMATION_SCHEMA.INNODB_TABLES t
  JOIN INFORMATION_SCHEMA.INNODB_INDEXES i
    ON i.TABLE_ID = t.TABLE_ID
  JOIN INFORMATION_SCHEMA.INNODB_BUFFER_PAGE_LRU bp
    ON bp.SPACE = i.SPACE
   AND bp.INDEX_NAME = i.NAME
  WHERE t.NAME = table_name
    AND i.NAME = index_name
    AND bp.PAGE_TYPE IN ('INDEX', 'RTREE_INDEX');

  RETURN pages;
END|

--delimiter ;
