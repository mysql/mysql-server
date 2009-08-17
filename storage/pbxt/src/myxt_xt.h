/* Copyright (c) 2005 PrimeBase Technologies GmbH
 *
 * PrimeBase XT
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 * 2006-05-16	Paul McCullagh
 *
 * H&G2JCtL
 *
 * These functions implement the parts of PBXT which must conform to the
 * key and row format used by MySQL. 
 */

#ifndef __xt_myxt_h__
#define __xt_myxt_h__

#include "xt_defs.h"
#include "table_xt.h"
#include "datadic_xt.h"

#ifndef MYSQL_VERSION_ID
#error MYSQL_VERSION_ID must be defined!
#endif

struct XTDictionary;
struct XTDatabase;
STRUCT_TABLE;
struct charset_info_st;

u_int		myxt_create_key_from_key(XTIndexPtr ind, xtWord1 *key, xtWord1 *old, u_int k_length);
u_int		myxt_create_key_from_row(XTIndexPtr ind, xtWord1 *key, xtWord1 *record, xtBool *no_duplicate);
u_int		myxt_create_foreign_key_from_row(XTIndexPtr ind, xtWord1 *key, xtWord1 *record, XTIndexPtr fkey_ind, xtBool *no_null);
u_int		myxt_get_key_length(XTIndexPtr ind, xtWord1 *b_value);
int			myxt_compare_key(XTIndexPtr ind, int search_flags, uint key_length, xtWord1 *key_value, xtWord1 *b_value);
u_int		myxt_key_seg_length(XTIndexSegRec *keyseg, u_int key_offset, xtWord1 *key_value);
xtBool		myxt_create_row_from_key(XTOpenTablePtr ot, XTIndexPtr ind, xtWord1 *key, u_int key_len, xtWord1 *record);
void		myxt_set_null_row_from_key(XTOpenTablePtr ot, XTIndexPtr ind, xtWord1 *record);
void		myxt_set_default_row_from_key(XTOpenTablePtr ot, XTIndexPtr ind, xtWord1 *record);
void		myxt_print_key(XTIndexPtr ind, xtWord1 *key_value);

xtWord4		myxt_store_row_length(XTOpenTablePtr ot, char *rec_buff);
xtBool		myxt_store_row(XTOpenTablePtr ot, XTTabRecInfoPtr rec_info, char *rec_buff);
size_t		myxt_load_row_length(XTOpenTablePtr ot, size_t buffer_size, xtWord1 *source_buf, u_int *ret_col_cnt);
xtBool		myxt_load_row(XTOpenTablePtr ot, xtWord1 *source_buf, xtWord1 *dest_buff, u_int col_cnt);
xtBool		myxt_find_column(XTOpenTablePtr ot, u_int *col_idx, const char *col_name);
void		myxt_get_column_name(XTOpenTablePtr ot, u_int col_idx, u_int len, char *col_name);
void		myxt_get_column_as_string(XTOpenTablePtr ot, char *buffer, u_int col_idx, u_int len, char *value);
xtBool		myxt_set_column(XTOpenTablePtr ot, char *buffer, u_int col_idx, const char *value, u_int len);
void		myxt_get_column_data(XTOpenTablePtr ot, char *buffer, u_int col_idx, char **value, size_t *len);

void		myxt_setup_dictionary(XTThreadPtr self, XTDictionary *dic);
xtBool		myxt_load_dictionary(XTThreadPtr self, struct XTDictionary *dic, struct XTDatabase *db, XTPathStrPtr tab_path);
void		myxt_free_dictionary(XTThreadPtr self, XTDictionary *dic);
void		myxt_move_dictionary(XTDictionaryPtr dic, XTDictionaryPtr source_dic);
XTDDTable	*myxt_create_table_from_table(XTThreadPtr self, STRUCT_TABLE *my_tab);

void		myxt_static_convert_identifier(XTThreadPtr self, struct charset_info_st *cs, char *from, char *to, size_t to_len);
char		*myxt_convert_identifier(XTThreadPtr self, struct charset_info_st *cs, char *from);
void		myxt_static_convert_table_name(XTThreadPtr self, char *from, char *to, size_t to_len);
char		*myxt_convert_table_name(XTThreadPtr self, char *from);
int			myxt_strcasecmp(char * a, char *b);
int			myxt_isspace(struct charset_info_st *cs, char a);
int			myxt_ispunct(struct charset_info_st *cs, char a);
int			myxt_isdigit(struct charset_info_st *cs, char a);

struct charset_info_st *myxt_getcharset(bool convert);

#ifdef XT_STREAMING
xtBool		myxt_use_blobs(XTOpenTablePtr ot, void **ret_pbms_table, xtWord1 *rec_buf);
void		myxt_unuse_blobs(XTOpenTablePtr ot, void *pbms_table);
xtBool		myxt_retain_blobs(XTOpenTablePtr ot, void *pbms_table, xtRecordID record);
void		myxt_release_blobs(XTOpenTablePtr ot, xtWord1 *rec_buf, xtRecordID record);
#endif

void		*myxt_create_thread();
void		myxt_destroy_thread(void *thread, xtBool end_threads);
XTThreadPtr	myxt_get_self();

int			myxt_statistics_fill_table(XTThreadPtr self, void *th, void *ta, void *co, MX_CONST void *ch);
void		myxt_get_status(XTThreadPtr self, XTStringBufferPtr strbuf);

class XTDDColumnFactory
{
public:
	static XTDDColumn *createFromMySQLField(XTThread *self, STRUCT_TABLE *, Field *);
};

#endif
