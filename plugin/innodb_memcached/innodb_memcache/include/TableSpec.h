#ifndef NDBMEMCACHE_TABLESPEC_H
#define NDBMEMCACHE_TABLESPEC_H

#ifndef __cplusplus
#error "This file is for C++ only"
#endif

#include "config.h"

/** @class TableSpec
 *  @brief In-memory representation of a containers record from the
 * configuration.
 */
class TableSpec {
 public:
  /* Public Constructors */
  TableSpec(int nkeys, int nvals);
  TableSpec(const char *db, const char *tab, int nkeys, int nvals);
  TableSpec(const char *sqltabname, const char *keycols, const char *valcols);
  TableSpec(const TableSpec &);

  /* Other Public Methods */
  ~TableSpec();
  void setTable(const char *db, const char *table);
  void setKeyColumns(const char *col1, ...);
  void setValueColumns(const char *col1, ...);

  /* Public instance variables */
  const char *schema_name;
  const char *table_name;
  const char *math_column;
  const char *flags_column;
  const char *cas_column;
  const char *exp_column;
  const char **const key_columns;
  const char **const value_columns;
  int nkeycols;
  int nvaluecols;
  uint32_t static_flags;

 private:
  /* private instance variables */
  struct {
    unsigned none : 1;
    unsigned schema_name : 1;
    unsigned table_name : 1;
    unsigned first_key : 1;
    unsigned all_key_cols : 1;
    unsigned first_val : 1;
    unsigned all_val_cols : 1;
    unsigned special_cols : 1;
  } must_free;

  /* private class methods */
  static int build_column_list(const char **const &array, const char *list);
};

/* Inline functions */

inline TableSpec::TableSpec(int nkeys, int nvals)
    : nkeycols(nkeys),
      nvaluecols(nvals),
      schema_name(0),
      table_name(0),
      math_column(0),
      flags_column(0),
      cas_column(0),
      exp_column(0),
      static_flags(0),
      key_columns(new const char *[nkeys]),
      value_columns(new const char *[nvals]) {
  must_free.none = 1;
};

inline TableSpec::TableSpec(const char *db, const char *tab, int nkeys,
                            int nvals)
    : schema_name(db),
      table_name(tab),
      nkeycols(nkeys),
      nvaluecols(nvals),
      static_flags(0),
      math_column(0),
      flags_column(0),
      cas_column(0),
      exp_column(0),
      key_columns(new const char *[nkeys]),
      value_columns(new const char *[nvals]) {
  must_free.none = 1;
};

inline void TableSpec::setTable(const char *db, const char *table) {
  schema_name = db;
  table_name = table;
}

#endif
