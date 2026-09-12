// Copyright 2026 Google LLC

#ifndef _INCLUDE_VECTOR0SUBTABLE_H_
#define _INCLUDE_VECTOR0SUBTABLE_H_

#include <error.h>
#include <sys/stat.h>

#include "dict0types.h"
#include <string>


namespace ib_vector {

/**
Given table name, table id and index id, we generate sub_table name
in this function. The sub table will be created in the same schema
of the parent table and the table name format is
vector_index_<table_id>_<index_id>

@param[in]    table_name   table name including db name
@param[in]    table_id     table id
@param[in]    index_id     index id
@return       std::string  generated sub table name
*/
std::string vector_generate_sub_table_name(const char *table_name,
                                  table_id_t table_id, space_index_t index_id);
/**
 This is wrapper around above function that generates sub table name for
  a given table and index object details
*/
std::string vector_generate_sub_table_name(dict_table_t *table,
                                  dict_index_t *index);
/**
This is wrapper around above function that generates sub table name for
a given table object details
*/
std::string vector_generate_sub_table_name(dict_table_t *table);

/**
  Creates in memory sub table , which is needed for supporting a
  vector index on the given table. row_mysql_lock_data_dictionary must have
  been called before this.

  Vector Index sub table will have following schema.
  CREATE TABLE vector_index_<table_id>_<index_id>(
                `partition_id` BIGINT  NOT NULL,
                `base_pk` varbinary(3072) NOT NULL,
                `content` blob,
                PRIMARY KEY subtable_pk(`partition_id`,`base_pk`),
                UNIQUE KEY `base_pk` (`base_pk`))
@param[in]    trx     transaction object
@param[in]    index   vector index that is getting created
@return       dict_table_t*  sub table id
*/
dict_table_t* create_vector_index_sub_table(trx_t *trx, dict_index_t *index);
/**
Creates dd::Table object for vector index sub table.
@param[in]    parent_table   base table object
@return       dberr_t return error in case of failure
*/
dberr_t create_vector_index_dd_sub_table(dict_table_t *parent_table);
/**
Drops vector index sub table.
@param[in]    parent_table   base table object
@param[in]    index          vector index that is getting dropped
@param[in]    trx            transaction object
@return       dberr_t return error in case of failure
*/
dberr_t drop_vector_index_sub_table(dict_table_t *parent_table,
                                    dict_index_t *index, trx_t *trx);
/**
Drops dd::Table object for vector index sub table.
@param[in]    parent_table   base table object
@param[in]    index          vector index that is getting dropped
@param[in]    trx            transaction object
@return       dberr_t return error in case of failure
*/
dberr_t drop_vector_index_dd_sub_table(dict_table_t *parent_table,
                                       dict_index_t *index, trx_t *trx);
/**
To pin vector index's sub table. Given table object, it will find out the
sub table name and pin it in memory.
@param[in]    table   base table object
*/
void pin_vector_index_sub_table(dict_table_t *table);
/**
To unpin vector index's sub table. Given table object, it will find out the
sub table name and pin it in memory. We will unpin only if there is an error
in the work flow. Otherwise, it will always be pinned.
@param[in]    table   base table object
*/
void unpin_vector_index_sub_table(dict_table_t *table, bool dict_locked);
/**
To lock vector index's sub table. Given table object, it will find out the
sub table name and lock (MDL exclusive) lock.
@param[in]    thd     THD object
@param[in]    table   base table object
*/
dberr_t lock_vector_index_sub_table(THD *thd, dict_table_t *table);
} /* namespace ib_vector */

#endif /* _INCLUDE_VECTOR0SUBTABLE_H_ */
