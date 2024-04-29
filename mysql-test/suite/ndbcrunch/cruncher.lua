#!/usr/bin/env sysbench


if sysbench.cmdline.command == nil then
   error("Command is required. Supported commands: prepare, run, " ..
            "cleanup, help")
end

-- Command line options
sysbench.cmdline.options = {
   table_size =
      {"Rows per table", 10000},
   tables =
      {"Number of tables", 1},
   primary_keys =
      { "Number of primary keys in table", 1 },
   blob_length =
      {"Length of blob column (this also changes column c to blob)", 0},
   table_nologging =
      {"Use NOLOGGING when creating table", ""},

   -- Options for being compatible with oltp_ scripts
   mysql_storage_engine =
      {"Storage engine used", "ndbcluster"},
   auto_inc =
      {"Use AUTO_INCREMENT as PRIMARY KEY, otherwise generate id.", true},
   secondary =
      {"Use a secondary index in place of the PRIMARY KEY", false},
    create_secondary =
      {"Create a secondary index in addition to the PRIMARY KEY", true},
}

local c_len = 4500  -- 4,5kB by default

function create_table(drv, con, table_num)
   local id_def
   if sysbench.opt.auto_inc then
     id_def = "INTEGER NOT NULL AUTO_INCREMENT"
   else
     id_def = "INTEGER NOT NULL"
   end

   local c_def = "varchar(6000)"
   if sysbench.opt.blob_length > 0 then
     if sysbench.opt.blob_length > 65535 then
       c_def = "longblob"
     else
       c_def = "blob"
     end
     c_len = sysbench.opt.blob_length
   end

   local tab_comment = "";
   if sysbench.opt.nologging then
     tab_comment = "COMMENT='NDB_TABLE=NOLOGGING=1'"
   end

   print(string.format("Creating table 'crunch%d'...", table_num))

   local query = string.format([[
CREATE TABLE crunch%d(
  id %s,
  k INTEGER DEFAULT '0' NOT NULL,
  c %s NOT NULL,
  pad CHAR(60) DEFAULT '' NOT NULL,
  PRIMARY KEY (id)
) engine %s %s]],
      table_num, id_def, c_def, sysbench.opt.mysql_storage_engine, tab_comment)

   con:query(query)

   if (sysbench.opt.table_size > 0) then
      print(string.format("Inserting %d records into 'crunch%d'",
                          sysbench.opt.table_size, table_num))
   end

   if sysbench.opt.auto_inc then
      query = "INSERT INTO crunch" .. table_num .. "(k, c, pad) VALUES"
   else
      query = "INSERT INTO crunch" .. table_num .. "(id, k, c, pad) VALUES"
   end

   con:bulk_insert_init(query)

   local c_val
   local pad_val

   local pad_value_template = "###########-###########-###########-" ..
     "###########-###########"

   for i = 1, sysbench.opt.table_size do

      c_val = sysbench.rand.string(string.rep("@", math.min(c_len, 1000)));
      pad_val = sysbench.rand.string(pad_value_template)

      if (sysbench.opt.auto_inc) then
         query = string.format("(%d, '%s', '%s')",
                               sysbench.rand.default(1, sysbench.opt.table_size),
                               c_val, pad_val)
      else
         query = string.format("(%d, %d, '%s', '%s')",
                               i,
                               sysbench.rand.default(1, sysbench.opt.table_size),
                               c_val, pad_val)
      end

      con:bulk_insert_next(query)
   end

   con:bulk_insert_done()

   if sysbench.opt.create_secondary then
      print(string.format("Creating a secondary index on 'crunch%d'...",
                          table_num))
      con:query(string.format("CREATE INDEX k_%d ON crunch%d(k)",
                              table_num, table_num))
   end
end

-- Prepare the dataset. This command supports parallel execution, i.e. will
-- benefit from executing with --threads > 1 as long as --tables > 1
function cmd_prepare()
   local drv = sysbench.sql.driver()
   local con = drv:connect()

   for i = sysbench.tid % sysbench.opt.threads + 1, sysbench.opt.tables,
   sysbench.opt.threads do
     create_table(drv, con, i)
   end
end

function cleanup()
   local drv = sysbench.sql.driver()
   local con = drv:connect()

   for i = 1, sysbench.opt.tables do
      print(string.format("Dropping table 'crunch%d'...", i))
      con:query("DROP TABLE IF EXISTS crunch" .. i )
   end
end

sysbench.cmdline.commands = {
   prepare = {cmd_prepare, sysbench.cmdline.PARALLEL_COMMAND},
}

function thread_init()
   -- init MySQL driver and connect
   drv = sysbench.sql.driver()
   con = drv:connect()
end

function thread_done()
  con:disconnect()
end

function event()
  local table_num = sysbench.rand.uniform(1, sysbench.opt.tables)
  local id = sysbench.rand.default(1, sysbench.opt.table_size)

  local c_data = sysbench.rand.string(string.rep("@", c_len));
  local q = string.format("UPDATE crunch%d SET c='%s' WHERE id=%d",
                          table_num, c_data, id )
  con:query(q)
end
