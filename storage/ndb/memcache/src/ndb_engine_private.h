/*
 Copyright (c) 2011, 2017, Oracle and/or its affiliates. All rights reserved.
 
 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License, version 2.0,
 as published by the Free Software Foundation.

 This program is also distributed with certain software (including
 but not limited to OpenSSL) that is licensed under separate terms,
 as designated in a particular file or component or in included license
 documentation.  The authors of MySQL hereby grant you an additional
 permission to link the program and your derivative works with the
 separately licensed software that they have included with MySQL.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License, version 2.0, for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */

#ifdef __cplusplus
extern "C" {
#endif


ndb_pipeline * get_my_pipeline_config(struct ndb_engine *);

void read_cmdline_options(struct ndb_engine *, struct default_engine *,
                          const char *);

int fetch_core_settings(struct ndb_engine *, struct default_engine *);

void release_and_free(struct workitem *);

ENGINE_ERROR_CODE stats_menu(ADD_STAT add_stat, const void *cookie);

/*************** Declarations of functions that implement 
                 the engine interface 
 *********************************************************/
static const engine_info* ndb_get_info(ENGINE_HANDLE* handle);

static ENGINE_ERROR_CODE ndb_initialize(ENGINE_HANDLE* handle,
                                        const char* config_str);

static void ndb_destroy(ENGINE_HANDLE* handle, bool force);

static ENGINE_ERROR_CODE ndb_allocate(ENGINE_HANDLE* handle,
                                      const void* cookie,
                                      item **item,
                                      const void* key,
                                      const size_t nkey,
                                      const size_t nbytes,
                                      const int flags,
                                      const rel_time_t exptime);

static ENGINE_ERROR_CODE ndb_remove(ENGINE_HANDLE* handle,
                                    const void* cookie,
                                    const void* key,
                                    const size_t nkey,
                                    uint64_t cas,
                                    uint16_t vbucket);

static void ndb_release(ENGINE_HANDLE* handle,
                        const void *cookie,
                        item* item);

static ENGINE_ERROR_CODE ndb_get(ENGINE_HANDLE* handle,
                                 const void* cookie,
                                 item** item,
                                 const void* key,
                                 const int nkey,
                                 uint16_t vbucket);

static ENGINE_ERROR_CODE ndb_get_stats(ENGINE_HANDLE* handle,
                                       const void *cookie,
                                       const char *stat_key,
                                       int nkey,
                                       ADD_STAT add_stat);

static void ndb_reset_stats(ENGINE_HANDLE* handle, 
                            const void *cookie);

static ENGINE_ERROR_CODE ndb_store(ENGINE_HANDLE* handle,
                                   const void *cookie,
                                   item* item,
                                   uint64_t *cas,
                                   ENGINE_STORE_OPERATION operation,
                                   uint16_t vbucket);

static ENGINE_ERROR_CODE ndb_arithmetic(ENGINE_HANDLE* handle,
                                        const void* cookie,
                                        const void* key,
                                        const int nkey,
                                        const bool increment,
                                        const bool create,
                                        const uint64_t delta,
                                        const uint64_t initial,
                                        const rel_time_t exptime,
                                        uint64_t *cas,
                                        uint64_t *result,
                                        uint16_t vbucket);

static ENGINE_ERROR_CODE ndb_flush(ENGINE_HANDLE* handle,
                                   const void* cookie, 
                                   time_t when);

static ENGINE_ERROR_CODE ndb_unknown_command(ENGINE_HANDLE* handle,
                                             const void* cookie,
                                             protocol_binary_request_header *request,
                                             ADD_RESPONSE response);

static bool ndb_get_item_info(ENGINE_HANDLE *handle, 
                              const void *cookie,
                              const item* item, 
                              item_info *item_info);

ENGINE_ERROR_CODE default_engine_create_instance(uint64_t,
                                                 GET_SERVER_API,
                                                 ENGINE_HANDLE**);

#ifdef __cplusplus
}
#endif
