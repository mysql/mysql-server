/*
   Copyright (c) 2003, 2018, Oracle and/or its affiliates. All rights reserved.

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

#ifndef GLOBAL_SIGNAL_NUMBERS_H
#define GLOBAL_SIGNAL_NUMBERS_H

#include <kernel_types.h>
/**
 * NOTE
 *
 * When adding a new signal, remember to update MAX_GSN and SignalNames.cpp
 */
const GlobalSignalNumber MAX_GSN = 827;

struct GsnName {
  GlobalSignalNumber gsn;
  const char * name;
};

extern const GsnName SignalNames[];
extern const GlobalSignalNumber NO_OF_SIGNAL_NAMES;

/**
 * These are used by API and kernel
 */
#define GSN_API_REGCONF                 1
#define GSN_API_REGREF                  2
#define GSN_API_REGREQ                  3

#define GSN_ATTRINFO                    4
#define GSN_TRANSID_AI                  5
#define GSN_KEYINFO                     6
#define GSN_READCONF                    7

#define GSN_TCKEY_FAILCONF              8
#define GSN_TCKEY_FAILREF               9
#define GSN_TCKEYCONF                   10
#define GSN_TCKEYREF                    11
#define GSN_TCKEYREQ                    12

#define GSN_TCROLLBACKCONF              13
#define GSN_TCROLLBACKREF               14
#define GSN_TCROLLBACKREQ               15
#define GSN_TCROLLBACKREP               16

#define GSN_TC_COMMITCONF               17
#define GSN_TC_COMMITREF                18
#define GSN_TC_COMMITREQ                19
#define GSN_TC_HBREP                    20

#define GSN_TRANSID_AI_R                21
#define GSN_KEYINFO20_R                 22

#define GSN_GET_TABINFOREF              23
#define GSN_GET_TABINFOREQ              24
#define GSN_GET_TABINFO_CONF            190

#define GSN_GET_TABLEID_REQ             683
#define GSN_GET_TABLEID_REF             684
#define GSN_GET_TABLEID_CONF            685

#define GSN_DIHNDBTAMPER                25
#define GSN_NODE_FAILREP                26
#define GSN_NF_COMPLETEREP              27

#define GSN_SCAN_NEXTREQ                28
#define GSN_SCAN_TABCONF                29
/* 30 unused */
#define GSN_SCAN_TABREF                 31
#define GSN_SCAN_TABREQ                 32
#define GSN_KEYINFO20                   33

#define GSN_TCRELEASECONF               34
#define GSN_TCRELEASEREF                35
#define GSN_TCRELEASEREQ                36

#define GSN_TCSEIZECONF                 37
#define GSN_TCSEIZEREF                  38
#define GSN_TCSEIZEREQ                  39

#define GSN_TCKEY_FAILREFCONF_R         40

#define GSN_DBINFO_SCANREQ              41
#define GSN_DBINFO_SCANCONF             42
#define GSN_DBINFO_SCANREF              43
#define GSN_DBINFO_TRANSID_AI           44

#define GSN_CONFIG_CHANGE_REQ           45
#define GSN_CONFIG_CHANGE_REF           46
#define GSN_CONFIG_CHANGE_CONF          47

#define GSN_CONFIG_CHANGE_IMPL_REQ      48
#define GSN_CONFIG_CHANGE_IMPL_REF      49
#define GSN_CONFIG_CHANGE_IMPL_CONF     50

#define GSN_CONFIG_CHECK_REQ            51
#define GSN_CONFIG_CHECK_REF            52
#define GSN_CONFIG_CHECK_CONF           53

#define GSN_GET_CONFIG_REQ        54
#define GSN_GET_CONFIG_REF        55
#define GSN_GET_CONFIG_CONF       56

/* 57 unused */
/* 58 unused */
/* 59 unused */
#define GSN_ALLOC_NODEID_REQ            60
#define GSN_ALLOC_NODEID_CONF           61
#define GSN_ALLOC_NODEID_REF            62
/* 63 unused */
/* 64 unused */
/* 65 unused */
/* 66 unused */

/**
 * These are used only by kernel
 */

#define GSN_ACC_ABORTCONF               67
/* 68 not unused */
/* 69 not unused */
#define GSN_UPDATE_FRAG_DIST_KEY_ORD    70
#define GSN_ACC_ABORTREQ                71
#define GSN_ACC_CHECK_SCAN              72
#define GSN_ACC_COMMITCONF              73
#define GSN_ACC_COMMITREQ               74
/* 75 unused */
/* 76 unused */

/* 79 unused */
/* 78 unused */
/* 77 unused */

/* 80 unused */
#define GSN_ACC_OVER_REC                81

/* 83 unused */
#define GSN_ACC_SCAN_INFO               84 /* local */
#define GSN_ALLOC_MEM_REQ               85 /* local */
#define GSN_ACC_SCANCONF                86
#define GSN_ACC_SCANREF                 87
#define GSN_ACC_SCANREQ                 88

#define GSN_RESTORE_LCP_REQ             91
#define GSN_RESTORE_LCP_REF             90
#define GSN_RESTORE_LCP_CONF            89

#define GSN_ACC_TO_CONF                 92
#define GSN_ACC_TO_REF                  93
#define GSN_ACC_TO_REQ                  94
#define GSN_ACCFRAGCONF                 95
#define GSN_ACCFRAGREF                  96
#define GSN_ACCFRAGREQ                  97
#define GSN_ACCKEYCONF                  98
#define GSN_ACCKEYREF                   99
#define GSN_ACCKEYREQ                   100
#define GSN_ACCMINUPDATE                101
#define GSN_ACCSEIZECONF                103
#define GSN_ACCSEIZEREF                 104
#define GSN_ACCSEIZEREQ                 105
#define GSN_ACCUPDATECONF               106
#define GSN_ACCUPDATEKEY                107
#define GSN_ACCUPDATEREF                108

#define GSN_ADD_FRAGCONF                109
#define GSN_ADD_FRAGREF                 110
#define GSN_ADD_FRAGREQ                 111

#define GSN_API_START_REP               120
#define GSN_API_FAILCONF                113
#define GSN_API_FAILREQ                 114
#define GSN_CNTR_START_REQ              115
/* 116 not unused */
#define GSN_CNTR_START_REF              117
#define GSN_CNTR_START_CONF             118
#define GSN_CNTR_START_REP              119
/* 120 not unused */
#define GSN_ROUTE_ORD                   121
#define GSN_NODE_VERSION_REP            122
/* 123 not unused */
#define GSN_FSSUSPENDORD                124 /* local */
#define GSN_CHECK_LCP_STOP              125
#define GSN_CLOSE_COMCONF               126 /* local */
#define GSN_CLOSE_COMREQ                127 /* local */
#define GSN_CM_ACKADD                   128 /* distr. */
#define GSN_ENABLE_COMCONF              129 /* local */
#define GSN_CM_ADD                      130 /* distr. */
/* 131 unused */
/* 132 not unused */
/* 133 not unused */
#define GSN_CM_HEARTBEAT                134 /* distr. */

#define GSN_PREPARE_COPY_FRAG_REQ       135
#define GSN_PREPARE_COPY_FRAG_REF       136
#define GSN_PREPARE_COPY_FRAG_CONF      137

#define GSN_CM_NODEINFOCONF             138 /* distr. */
#define GSN_CM_NODEINFOREF              139 /* distr. */
#define GSN_CM_NODEINFOREQ              140 /* distr. */
#define GSN_CM_REGCONF                  141 /* distr. */
#define GSN_CM_REGREF                   142 /* distr. */
#define GSN_CM_REGREQ                   143 /* distr. */
/* 144 not unused */
/* 145 not unused */
/* 146 not unused */
#define GSN_CM_ADD_REP                  147 /* local */
/* 148 not unused  */
/* 149 not unused  */
/* 150 not unused  */
#define GSN_CNTR_WAITREP                151 /* distr. */
#define GSN_COMMIT                      152
#define GSN_COMMIT_FAILCONF             153
#define GSN_COMMIT_FAILREQ              154
#define GSN_COMMITCONF                  155
#define GSN_COMMITREQ                   156
#define GSN_COMMITTED                   157
#define GSN_COMPLETE                    159
#define GSN_COMPLETECONF                160
#define GSN_COMPLETED                   161
#define GSN_COMPLETEREQ                 162
#define GSN_CONNECT_REP                 163
#define GSN_CONTINUEB                   164
/* 165 not unused */
#define GSN_COPY_ACTIVECONF             166
#define GSN_COPY_ACTIVEREF              167
#define GSN_COPY_ACTIVEREQ              168
#define GSN_COPY_FRAGCONF               169
#define GSN_COPY_FRAGREF                170
#define GSN_COPY_FRAGREQ                171
#define GSN_COPY_GCICONF                172
#define GSN_COPY_GCIREQ                 173
/* 174 used to be COPY_STATECONF, no longer used */
/* 175 used to be COPY_STATEREQ, no longer used */
#define GSN_COPY_TABCONF                176
#define GSN_COPY_TABREQ                 177
#define GSN_UPDATE_FRAG_STATECONF       178
#define GSN_UPDATE_FRAG_STATEREF        179
#define GSN_UPDATE_FRAG_STATEREQ        180
#define GSN_DEBUG_SIG                   181
#define GSN_DIH_SCAN_TAB_REQ            182
#define GSN_DIH_SCAN_TAB_REF            183
#define GSN_DIH_SCAN_TAB_CONF           184
#define GSN_DIH_SCAN_TAB_COMPLETE_REP   287
#define GSN_DIADDTABCONF                185
#define GSN_DIADDTABREF                 186
#define GSN_DIADDTABREQ                 187
/* 188 not unused */
/* 189 not unused */
/* 190 not unused */
#define GSN_DICTSTARTCONF               191
#define GSN_DICTSTARTREQ                192

#define GSN_LIST_TABLES_REQ             193
#define GSN_LIST_TABLES_CONF            194

#define GSN_ABORT                       195
#define GSN_ABORTCONF                   196
#define GSN_ABORTED                     197
#define GSN_ABORTREQ                    198

/******************************************
 * DROP TABLE
 * 
 */

/**
 * This is drop table's public interface
 */
#define GSN_DROP_TABLE_REQ               82
#define GSN_DROP_TABLE_REF              102
#define GSN_DROP_TABLE_CONF             112

/**
 * This is used for implementing drop table
 */
#define GSN_PREP_DROP_TAB_REQ           199
#define GSN_PREP_DROP_TAB_REF           200
#define GSN_PREP_DROP_TAB_CONF          201

#define GSN_DROP_TAB_REQ                202
#define GSN_DROP_TAB_REF                203
#define GSN_DROP_TAB_CONF               204

#define GSN_DIH_GET_TABINFO_REQ         208 /* distr. */
#define GSN_DIH_GET_TABINFO_REF         209 /* distr. */
#define GSN_DIH_GET_TABINFO_CONF        232 /* distr. */

/*****************************************/

#define GSN_UPDATE_TOCONF               205
#define GSN_UPDATE_TOREF                206
#define GSN_UPDATE_TOREQ                207

#define GSN_DIGETNODESCONF              210
#define GSN_DIGETNODESREF               211
#define GSN_DIGETNODESREQ               212

/*
 Signal 213, 214, 215 no longer used, was
 DIH_SCAN_GET_NODES_REQ/CONF/REF
*/

#define GSN_DIH_RESTARTCONF             217
#define GSN_DIH_RESTARTREF              218
#define GSN_DIH_RESTARTREQ              219

/* 220 not unused */
/* 221 not unused */
/* 222 not unused */

#define GSN_EMPTY_LCP_REQ               223
#define GSN_EMPTY_LCP_CONF              224
#define GSN_EMPTY_LCP_REP               223 // local (LQH - DIH)

#define GSN_SCHEMA_INFO                 225
#define GSN_SCHEMA_INFOCONF             226

#define GSN_MASTER_GCPCONF              227
#define GSN_MASTER_GCPREF               228
#define GSN_MASTER_GCPREQ               229

/* 230 not unused */
/* 231 not unused */

/* 232 not unused */
/* 233 unused */
/* 234 unused */
#define GSN_DISCONNECT_REP              235

#define GSN_FIRE_TRIG_REQ               236
#define GSN_FIRE_TRIG_REF               237
#define GSN_FIRE_TRIG_CONF              238

#define GSN_DIVERIFYCONF                239
#define GSN_DIVERIFYREF                 240
#define GSN_DIVERIFYREQ                 241
#define GSN_ENABLE_COMREQ               242 /* local */
#define GSN_END_LCPCONF                 243
#define GSN_END_LCPREQ                  244
#define GSN_END_TOCONF                  245
#define GSN_END_TOREQ                   246
#define GSN_END_TOREF                   286
#define GSN_EVENT_REP                   247
#define GSN_EXEC_FRAGCONF               248
#define GSN_EXEC_FRAGREF                249
#define GSN_EXEC_FRAGREQ                250
#define GSN_EXEC_SRCONF                 251
#define GSN_EXEC_SRREQ                  252
#define GSN_EXPANDCHECK2                253
#define GSN_FAIL_REP                    254
#define GSN_FSCLOSECONF                 255
#define GSN_FSCLOSEREF                  256
#define GSN_FSCLOSEREQ                  257
#define GSN_FSAPPENDCONF                258
#define GSN_FSOPENCONF                  259
#define GSN_FSOPENREF                   260
#define GSN_FSOPENREQ                   261
#define GSN_FSREADCONF                  262
#define GSN_FSREADREF                   263
#define GSN_FSREADREQ                   264
#define GSN_FSSYNCCONF                  265
#define GSN_FSSYNCREF                   266
#define GSN_FSSYNCREQ                   267
#define GSN_FSAPPENDREQ                 268
#define GSN_FSAPPENDREF                 269
#define GSN_FSWRITECONF                 270
#define GSN_FSWRITEREF                  271
#define GSN_FSWRITEREQ                  272
#define GSN_GCP_ABORT                   273
#define GSN_GCP_ABORTED                 274
#define GSN_GCP_COMMIT                  275
#define GSN_GCP_NODEFINISH              276
#define GSN_GCP_NOMORETRANS             277
#define GSN_GCP_PREPARE                 278
#define GSN_GCP_PREPARECONF             279
#define GSN_GCP_PREPAREREF              280
#define GSN_GCP_SAVECONF                281
#define GSN_GCP_SAVEREF                 282
#define GSN_GCP_SAVEREQ                 283
#define GSN_GCP_TCFINISHED              284

#define GSN_UPGRADE_PROTOCOL_ORD        285

/* 286 not unused */
/* 287 not unused */
#define GSN_GETGCICONF                  288
#define GSN_GETGCIREQ                   289
#define GSN_HOT_SPAREREP                290
#define GSN_INCL_NODECONF               291
#define GSN_INCL_NODEREF                292
#define GSN_INCL_NODEREQ                293

#define GSN_LCP_PREPARE_REQ             296
#define GSN_LCP_PREPARE_REF             295
#define GSN_LCP_PREPARE_CONF            294

#define GSN_CREATE_HASH_MAP_REQ         297
#define GSN_CREATE_HASH_MAP_REF         298
#define GSN_CREATE_HASH_MAP_CONF        299

#define GSN_SHRINKCHECK2                301
#define GSN_GET_SCHEMA_INFOREQ          302
/* 303 not unused */
/* 304 not unused */
#define GSN_ALLOC_MEM_REF               305 /* local */
#define GSN_LQH_TRANSCONF               306
#define GSN_LQH_TRANSREQ                307
#define GSN_LQHADDATTCONF               308
#define GSN_LQHADDATTREF                309
#define GSN_LQHADDATTREQ                310
#define GSN_LQHFRAGCONF                 311
#define GSN_LQHFRAGREF                  312
#define GSN_LQHFRAGREQ                  313
#define GSN_LQHKEYCONF                  314
#define GSN_LQHKEYREF                   315
#define GSN_LQHKEYREQ                   316

#define GSN_MASTER_LCPCONF              318
#define GSN_MASTER_LCPREF               319
#define GSN_MASTER_LCPREQ               320

#define GSN_MEMCHECKCONF                321
#define GSN_MEMCHECKREQ                 322
#define GSN_NDB_FAILCONF                323
#define GSN_NDB_STARTCONF               324
#define GSN_NDB_STARTREF                325
#define GSN_NDB_STARTREQ                326
#define GSN_NDB_STTOR                   327
#define GSN_NDB_STTORRY                 328
#define GSN_NDB_TAMPER                  329
#define GSN_NEXT_SCANCONF               330
#define GSN_NEXT_SCANREF                331
#define GSN_NEXT_SCANREQ                332
#define GSN_ALLOC_MEM_CONF              333 /* local */

#define GSN_READ_CONFIG_REQ             334 /* new name for sizealt, local */
#define GSN_READ_CONFIG_CONF            335 /* new name for sizealt, local */

#define GSN_COPY_DATA_REQ               336
#define GSN_COPY_DATA_REF               337
#define GSN_COPY_DATA_CONF              338

#define GSN_EXPAND_CLNT                 340
#define GSN_OPEN_COMORD                 341
#define GSN_PACKED_SIGNAL               342
#define GSN_PREP_FAILCONF               343
#define GSN_PREP_FAILREF                344
#define GSN_PREP_FAILREQ                345
#define GSN_PRES_TOCONF                 346
#define GSN_PRES_TOREQ                  347
#define GSN_READ_NODESCONF              348
#define GSN_READ_NODESREF               349
#define GSN_READ_NODESREQ               350
#define GSN_SCAN_FRAGCONF               351
#define GSN_SCAN_FRAGREF                352
#define GSN_SCAN_FRAGREQ                353
#define GSN_SCAN_HBREP                  354
#define GSN_SCAN_PROCCONF               355
#define GSN_SCAN_PROCREQ                356
#define GSN_SEND_PACKED                 357
#define GSN_SET_LOGLEVELORD             358

/* 359 used to be LQH_ALLOCREQ, no longer used */
/* 360 used to be TUP_ALLOCREQ, no longer used */
#define GSN_TUP_DEALLOCREQ              361

/* 362 not unused */

#define GSN_TUP_WRITELOG_REQ            363
#define GSN_LQH_WRITELOG_REQ            364

#define GSN_LCP_FRAG_REP                300
#define GSN_LCP_FRAG_ORD                365
#define GSN_LCP_COMPLETE_REP            158

#define GSN_START_LCP_REQ               317
#define GSN_START_LCP_CONF              366

#define GSN_UNBLO_DICTCONF              367
#define GSN_UNBLO_DICTREQ               368
#define GSN_START_COPYCONF              369
#define GSN_START_COPYREF               370
#define GSN_START_COPYREQ               371
#define GSN_START_EXEC_SR               372
#define GSN_START_FRAGCONF              373
#define GSN_START_FRAGREF               374
#define GSN_START_FRAGREQ               375
#define GSN_START_LCP_REF               376
#define GSN_START_LCP_ROUND             377
#define GSN_START_MECONF                378
#define GSN_START_MEREF                 379
#define GSN_START_MEREQ                 380
#define GSN_START_PERMCONF              381
#define GSN_START_PERMREF               382
#define GSN_START_PERMREQ               383
#define GSN_START_PERMREP               422
#define GSN_START_RECCONF               384
#define GSN_START_RECREF                385
#define GSN_START_RECREQ                386

#define GSN_START_TOCONF                387
#define GSN_START_TOREQ                 388
#define GSN_START_TOREF                 421

#define GSN_STORED_PROCCONF             389
#define GSN_STORED_PROCREF              390
#define GSN_STORED_PROCREQ              391
#define GSN_STTOR                       392
#define GSN_STTORRY                     393
#define GSN_BACKUP_TRIG_REQ             394
#define GSN_SYSTEM_ERROR                395
#define GSN_TAB_COMMITCONF              396
#define GSN_TAB_COMMITREF               397
#define GSN_TAB_COMMITREQ               398
#define GSN_TAKE_OVERTCCONF             399
#define GSN_TAKE_OVERTCREQ              400
#define GSN_TC_CLOPSIZECONF             401
#define GSN_TC_CLOPSIZEREQ              402
#define GSN_TC_SCHVERCONF               403
#define GSN_TC_SCHVERREQ                404
#define GSN_TCGETOPSIZECONF             405
#define GSN_TCGETOPSIZEREQ              406
#define GSN_TEST_ORD                    407
#define GSN_TESTSIG                     408
#define GSN_TIME_SIGNAL                 409
#define GSN_TUP_ABORTREQ                414
#define GSN_TUP_ADD_ATTCONF             415
#define GSN_TUP_ADD_ATTRREF             416
#define GSN_TUP_ADD_ATTRREQ             417
#define GSN_TUP_ATTRINFO                418
#define GSN_TUP_COMMITREQ               419

/* 421 not unused */
/* 422 not unused */
#define GSN_COPY_DATA_IMPL_REQ          423 /* local */
#define GSN_COPY_DATA_IMPL_REF          424 /* local */
#define GSN_COPY_DATA_IMPL_CONF         425 /* local */

#define GSN_DROP_FRAG_REQ               426 /* local */
#define GSN_DROP_FRAG_REF               427 /* local */
#define GSN_DROP_FRAG_CONF              428 /* local */
#define GSN_LOCAL_ROUTE_ORD             429 /* local */
/* 430 unused */
#define GSN_TUPFRAGCONF                 431
#define GSN_TUPFRAGREF                  432
#define GSN_TUPFRAGREQ                  433
#define GSN_TUPKEYCONF                  434
#define GSN_TUPKEYREF                   435
#define GSN_TUPKEYREQ                   436
#define GSN_TUPRELEASECONF              437
#define GSN_TUPRELEASEREF               438
#define GSN_TUPRELEASEREQ               439
#define GSN_TUPSEIZECONF                440
#define GSN_TUPSEIZEREF                 441
#define GSN_TUPSEIZEREQ                 442

#define GSN_ABORT_ALL_REQ               445
#define GSN_ABORT_ALL_REF               446
#define GSN_ABORT_ALL_CONF              447

/* 448 not unused - formerly GSN_STATISTICS_REQ */
#define GSN_STOP_ORD                    449
#define GSN_TAMPER_ORD                  450
/* 451 not unused - formerly GSN_SET_VAR_REQ  */
/* 452 not unused - formerly GSN_SET_VAR_CONF */
/* 453 not unused - formerly GSN_SET_VAR_REF  */
/* 454 not unused - formerly GSN_STATISTICS_CONF */

#define GSN_START_ORD                   455
/* 457 not unused */

#define GSN_EVENT_SUBSCRIBE_REQ         458
#define GSN_EVENT_SUBSCRIBE_CONF        459
#define GSN_EVENT_SUBSCRIBE_REF         460

/* NODE_PING signals */
#define GSN_NODE_PING_REQ               461 /* distr. */
#define GSN_NODE_PING_CONF              462 /* distr. */

#define GSN_CANCEL_SUBSCRIPTION_REQ     463
/* 464 unused */

#define GSN_DUMP_STATE_ORD              465

#define GSN_START_INFOREQ               466
#define GSN_START_INFOREF               467
#define GSN_START_INFOCONF              468

#define GSN_TC_COMMIT_ACK               469
#define GSN_REMOVE_MARKER_ORD           470

#define GSN_CHECKNODEGROUPSREQ          471
#define GSN_CHECKNODEGROUPSCONF         472

/* 473 unused */
#define GSN_ARBIT_PREPREQ               474
#define GSN_ARBIT_PREPCONF              475
#define GSN_ARBIT_PREPREF               476
#define GSN_ARBIT_STARTREQ              477
#define GSN_ARBIT_STARTCONF             478
#define GSN_ARBIT_STARTREF              479
#define GSN_ARBIT_CHOOSEREQ             480
#define GSN_ARBIT_CHOOSECONF            481
#define GSN_ARBIT_CHOOSEREF             482
#define GSN_ARBIT_STOPORD               483
#define GSN_ARBIT_STOPREP               484

#define GSN_BLOCK_COMMIT_ORD            485
#define GSN_UNBLOCK_COMMIT_ORD          486

#define GSN_NODE_START_REP              502
#define GSN_NODE_STATE_REP              487
#define GSN_CHANGE_NODE_STATE_REQ       488
#define GSN_CHANGE_NODE_STATE_CONF      489

#define GSN_DIH_SWITCH_REPLICA_REQ      490
#define GSN_DIH_SWITCH_REPLICA_CONF     491
#define GSN_DIH_SWITCH_REPLICA_REF      492

#define GSN_STOP_PERM_REQ               493
#define GSN_STOP_PERM_REF               494
#define GSN_STOP_PERM_CONF              495

#define GSN_STOP_ME_REQ                 496
#define GSN_STOP_ME_REF                 497
#define GSN_STOP_ME_CONF                498

#define GSN_WAIT_GCP_REQ                499
#define GSN_WAIT_GCP_REF                500
#define GSN_WAIT_GCP_CONF               501

/* 502 used */

/**
 * Trigger and index signals
 */

/**
 * These are used by API and kernel
 */
#define GSN_TRIG_ATTRINFO               503
#define GSN_CREATE_TRIG_REQ             504
#define GSN_CREATE_TRIG_CONF            505
#define GSN_CREATE_TRIG_REF             506
#define GSN_ALTER_TRIG_REQ              507
#define GSN_ALTER_TRIG_CONF             508
#define GSN_ALTER_TRIG_REF              509
#define GSN_CREATE_INDX_REQ             510
#define GSN_CREATE_INDX_CONF            511
#define GSN_CREATE_INDX_REF             512
#define GSN_DROP_TRIG_REQ               513
#define GSN_DROP_TRIG_CONF              514
#define GSN_DROP_TRIG_REF               515
#define GSN_DROP_INDX_REQ               516
#define GSN_DROP_INDX_CONF              517
#define GSN_DROP_INDX_REF               518
#define GSN_TCINDXREQ                   519
#define GSN_TCINDXCONF                  520
#define GSN_TCINDXREF                   521
#define GSN_INDXKEYINFO                 522
#define GSN_INDXATTRINFO                523
#define GSN_TCINDXNEXTREQ               524
#define GSN_TCINDXNEXTCONF              525
#define GSN_TCINDXNEXREF                526
#define GSN_FIRE_TRIG_ORD               527
#define GSN_FIRE_TRIG_ORD_L             123 /* local from TUP to SUMA */

/**
 * These are used only by kernel
 */
#define GSN_BUILDINDXREQ                528
#define GSN_BUILDINDXCONF               529
#define GSN_BUILDINDXREF                530

/**
 * Backup interface
 */
#define GSN_BACKUP_REQ                  531
#define GSN_BACKUP_DATA                 532
#define GSN_BACKUP_REF                  533
#define GSN_BACKUP_CONF                 534

#define GSN_ABORT_BACKUP_ORD            535

#define GSN_BACKUP_ABORT_REP            536
#define GSN_BACKUP_COMPLETE_REP         537
#define GSN_BACKUP_NF_COMPLETE_REP      538

/**
 * Internal backup signals
 */
#define GSN_DEFINE_BACKUP_REQ           539
#define GSN_DEFINE_BACKUP_REF           540
#define GSN_DEFINE_BACKUP_CONF          541

#define GSN_START_BACKUP_REQ            542
#define GSN_START_BACKUP_REF            543
#define GSN_START_BACKUP_CONF           544

#define GSN_BACKUP_FRAGMENT_REQ         545
#define GSN_BACKUP_FRAGMENT_REF         546
#define GSN_BACKUP_FRAGMENT_CONF        547

#define GSN_BACKUP_FRAGMENT_COMPLETE_REP 575

#define GSN_STOP_BACKUP_REQ             548
#define GSN_STOP_BACKUP_REF             549
#define GSN_STOP_BACKUP_CONF            550

/**
 * Used for master take-over / API status request
 */
#define GSN_BACKUP_STATUS_REQ           551
#define GSN_BACKUP_STATUS_REF           116
#define GSN_BACKUP_STATUS_CONF          165

/**
 * Db sequence signals
 */
#define GSN_UTIL_SEQUENCE_REQ           552
#define GSN_UTIL_SEQUENCE_REF           553
#define GSN_UTIL_SEQUENCE_CONF          554

#define GSN_FSREMOVEREQ                 555
#define GSN_FSREMOVEREF                 556
#define GSN_FSREMOVECONF                557

#define GSN_UTIL_PREPARE_REQ            558
#define GSN_UTIL_PREPARE_CONF           559
#define GSN_UTIL_PREPARE_REF            560

#define GSN_UTIL_EXECUTE_REQ            561
#define GSN_UTIL_EXECUTE_CONF           562
#define GSN_UTIL_EXECUTE_REF            563

#define GSN_UTIL_RELEASE_REQ            564
#define GSN_UTIL_RELEASE_CONF           565
#define GSN_UTIL_RELEASE_REF            566

/**
 * When dropping a long signal due to lack of memory resources
 */
#define GSN_SIGNAL_DROPPED_REP          567
#define GSN_CONTINUE_FRAGMENTED         568

/**
 * In multithreaded ndbd, sent from crashing thread to other threads to make
 * them stop prior to generating trace dump files.
 */
#define GSN_STOP_FOR_CRASH              761

/* Sent from BACKUP to DICT to lock a table during backup. */
#define GSN_BACKUP_LOCK_TAB_REQ         762
#define GSN_BACKUP_LOCK_TAB_CONF        763
#define GSN_BACKUP_LOCK_TAB_REF         764

/**
 * Suma participant interface
 */
#define GSN_SUB_REMOVE_REQ              569
#define GSN_SUB_REMOVE_REF              570
#define GSN_SUB_REMOVE_CONF             571
#define GSN_SUB_STOP_REQ                572
#define GSN_SUB_STOP_REF                573
#define GSN_SUB_STOP_CONF               574
/*                                      575 used */
#define GSN_SUB_CREATE_REQ              576
#define GSN_SUB_CREATE_REF              577
#define GSN_SUB_CREATE_CONF             578
#define GSN_SUB_START_REQ               579
#define GSN_SUB_START_REF               580
#define GSN_SUB_START_CONF              581
#define GSN_SUB_SYNC_REQ                582
#define GSN_SUB_SYNC_REF                583
#define GSN_SUB_SYNC_CONF               584
/*                                      585 unused */
#define GSN_SUB_TABLE_DATA              586

#define GSN_CREATE_TABLE_REQ            587
#define GSN_CREATE_TABLE_REF            588
#define GSN_CREATE_TABLE_CONF           589

#define GSN_ALTER_TABLE_REQ             624
#define GSN_ALTER_TABLE_REF             625
#define GSN_ALTER_TABLE_CONF            626

#define GSN_SUB_SYNC_CONTINUE_REQ       590
#define GSN_SUB_SYNC_CONTINUE_REF       591
#define GSN_SUB_SYNC_CONTINUE_CONF      592
#define GSN_SUB_GCP_COMPLETE_REP        593

#define GSN_CREATE_FRAGMENTATION_REQ    594
#define GSN_CREATE_FRAGMENTATION_REF    595
#define GSN_CREATE_FRAGMENTATION_CONF   596

#define GSN_CREATE_TAB_REQ              597
#define GSN_CREATE_TAB_REF              598
#define GSN_CREATE_TAB_CONF             599

#define GSN_ALTER_TAB_REQ               600
#define GSN_ALTER_TAB_REF               601
#define GSN_ALTER_TAB_CONF              602

#define GSN_ALTER_INDX_REQ              603
#define GSN_ALTER_INDX_REF              604
#define GSN_ALTER_INDX_CONF             605

#define GSN_ALTER_TABLE_REP             606
#define GSN_API_BROADCAST_REP           607

#define GSN_SYNC_THREAD_REQ             608
#define GSN_SYNC_THREAD_CONF            609

#define GSN_SYNC_REQ                    610
#define GSN_SYNC_REF                    611
#define GSN_SYNC_CONF                   612

#define GSN_SYNC_PATH_REQ               613
#define GSN_SYNC_PATH_CONF              614

#define GSN_LCP_STATUS_REQ              615
#define GSN_LCP_STATUS_CONF             616
#define GSN_LCP_STATUS_REF              617

#define GSN_CHECK_NODE_RESTARTREQ       618
#define GSN_CHECK_NODE_RESTARTCONF      619

#define GSN_GET_CPU_USAGE_REQ           620
#define GSN_OVERLOAD_STATUS_REP         621
#define GSN_SEND_THREAD_STATUS_REP      622
#define GSN_NODE_OVERLOAD_STATUS_ORD    623

#define GSN_CREATE_FK_REQ               627
#define GSN_CREATE_FK_REF               628
#define GSN_CREATE_FK_CONF              629

#define GSN_DROP_FK_REQ                 630
#define GSN_DROP_FK_REF                 631
#define GSN_DROP_FK_CONF                632

#define GSN_CREATE_FK_IMPL_REQ          633
#define GSN_CREATE_FK_IMPL_REF          634
#define GSN_CREATE_FK_IMPL_CONF         635

#define GSN_DROP_FK_IMPL_REQ            636
#define GSN_DROP_FK_IMPL_REF            637
#define GSN_DROP_FK_IMPL_CONF           638

#define GSN_BUILD_FK_REQ                639
#define GSN_BUILD_FK_REF                640
#define GSN_BUILD_FK_CONF               641

#define GSN_BUILD_FK_IMPL_REQ           642
#define GSN_BUILD_FK_IMPL_REF           643
#define GSN_BUILD_FK_IMPL_CONF          644

#define GSN_645
#define GSN_646
#define GSN_647
#define GSN_648
#define GSN_649

#define GSN_SET_WAKEUP_THREAD_ORD       657
#define GSN_WAKEUP_THREAD_ORD           658
#define GSN_SEND_WAKEUP_THREAD_ORD      659

#define GSN_UTIL_CREATE_LOCK_REQ        132
#define GSN_UTIL_CREATE_LOCK_REF        133
#define GSN_UTIL_CREATE_LOCK_CONF       188

#define GSN_UTIL_DESTROY_LOCK_REQ       189
#define GSN_UTIL_DESTROY_LOCK_REF       220
#define GSN_UTIL_DESTROY_LOCK_CONF      221

#define GSN_UTIL_LOCK_REQ               222
#define GSN_UTIL_LOCK_REF               230
#define GSN_UTIL_LOCK_CONF              231

#define GSN_UTIL_UNLOCK_REQ             303
#define GSN_UTIL_UNLOCK_REF             304
#define GSN_UTIL_UNLOCK_CONF            362

/* SUMA */
#define GSN_CREATE_SUBID_REQ            661      
#define GSN_CREATE_SUBID_REF            662      
#define GSN_CREATE_SUBID_CONF           663      

/* used 664 */
/* used 665 */
/* used 666 */
/* used 667 */
/* used 668 */
/* used 669 */

/*
 * TUX
 */
#define GSN_TUXFRAGREQ                  670
#define GSN_TUXFRAGCONF                 671
#define GSN_TUXFRAGREF                  672
#define GSN_TUX_ADD_ATTRREQ             673
#define GSN_TUX_ADD_ATTRCONF            674
#define GSN_TUX_ADD_ATTRREF             675

/*
 * REP
 */
#define GSN_REP_DISCONNECT_REP          676

#define GSN_TUX_MAINT_REQ               677
#define GSN_TUX_MAINT_CONF              678
#define GSN_TUX_MAINT_REF               679

/**
 * from mgmtsrvr to  NDBCNTR
 */
#define GSN_RESUME_REQ                  682
#define GSN_STOP_REQ                    443
#define GSN_STOP_REF                    444
#define GSN_STOP_CONF                   456
#define GSN_API_VERSION_REQ             697
#define GSN_API_VERSION_CONF            698

/* not used                             686 */
/* not used                             687 */
/* not used                             689 */
/* not used                             690 */

/**
 * SUMA restart protocol
 */
#define GSN_SUMA_START_ME_REQ           691
#define GSN_SUMA_START_ME_REF           694
#define GSN_SUMA_START_ME_CONF          695
#define GSN_SUMA_HANDOVER_REQ           692
#define GSN_SUMA_HANDOVER_REF           696
#define GSN_SUMA_HANDOVER_CONF          693

/* used 694 */
/* used 695 */
/* used 696 */

/*
 * EVENT Signals
 */
#define GSN_SUB_GCP_COMPLETE_ACK        699

#define GSN_CREATE_EVNT_REQ             700
#define GSN_CREATE_EVNT_CONF            701
#define GSN_CREATE_EVNT_REF             702

#define GSN_DROP_EVNT_REQ               703
#define GSN_DROP_EVNT_CONF              704
#define GSN_DROP_EVNT_REF               705

#define GSN_TUX_BOUND_INFO		710

#define GSN_ACC_LOCKREQ			711
#define GSN_READ_PSEUDO_REQ             712

/**
 * Filegroup 
 */
#define GSN_CREATE_FILEGROUP_REQ        713
#define GSN_CREATE_FILEGROUP_REF        714
#define GSN_CREATE_FILEGROUP_CONF       715

#define GSN_CREATE_FILE_REQ             716
#define GSN_CREATE_FILE_REF             717
#define GSN_CREATE_FILE_CONF            718

#define GSN_DROP_FILEGROUP_REQ          719
#define GSN_DROP_FILEGROUP_REF          720
#define GSN_DROP_FILEGROUP_CONF         721

#define GSN_DROP_FILE_REQ               722
#define GSN_DROP_FILE_REF               723
#define GSN_DROP_FILE_CONF              724

#define GSN_CREATE_FILEGROUP_IMPL_REQ   725
#define GSN_CREATE_FILEGROUP_IMPL_REF   726
#define GSN_CREATE_FILEGROUP_IMPL_CONF  727

#define GSN_CREATE_FILE_IMPL_REQ        728
#define GSN_CREATE_FILE_IMPL_REF        729
#define GSN_CREATE_FILE_IMPL_CONF       730

#define GSN_ALLOC_EXTENT_REQ             68
#define GSN_FREE_EXTENT_REQ              69

#define GSN_DROP_FILEGROUP_IMPL_REQ     664
#define GSN_DROP_FILEGROUP_IMPL_REF     665
#define GSN_DROP_FILEGROUP_IMPL_CONF    666

#define GSN_DROP_FILE_IMPL_REQ          667
#define GSN_DROP_FILE_IMPL_REF          668
#define GSN_DROP_FILE_IMPL_CONF         669

/* DICT master takeover signals */
#define GSN_DICT_TAKEOVER_REQ           765
#define GSN_DICT_TAKEOVER_REF           766
#define GSN_DICT_TAKEOVER_CONF          767


/* DICT LOCK signals */
#define GSN_DICT_LOCK_REQ               410
#define GSN_DICT_LOCK_CONF              411
#define GSN_DICT_LOCK_REF               412
#define GSN_DICT_UNLOCK_ORD             420

#define GSN_SCHEMA_TRANS_BEGIN_REQ      731
#define GSN_SCHEMA_TRANS_BEGIN_CONF     732
#define GSN_SCHEMA_TRANS_BEGIN_REF      733
#define GSN_SCHEMA_TRANS_END_REQ        734
#define GSN_SCHEMA_TRANS_END_CONF       735
#define GSN_SCHEMA_TRANS_END_REF        736
#define GSN_SCHEMA_TRANS_END_REP        768
#define GSN_SCHEMA_TRANS_IMPL_REQ       737
#define GSN_SCHEMA_TRANS_IMPL_CONF      738
#define GSN_SCHEMA_TRANS_IMPL_REF       739

#define GSN_CREATE_TRIG_IMPL_REQ        740
#define GSN_CREATE_TRIG_IMPL_CONF       741
#define GSN_CREATE_TRIG_IMPL_REF        742
#define GSN_DROP_TRIG_IMPL_REQ          743
#define GSN_DROP_TRIG_IMPL_CONF         744
#define GSN_DROP_TRIG_IMPL_REF          745
#define GSN_ALTER_TRIG_IMPL_REQ         746
#define GSN_ALTER_TRIG_IMPL_CONF        747
#define GSN_ALTER_TRIG_IMPL_REF         748

#define GSN_CREATE_INDX_IMPL_REQ        749
#define GSN_CREATE_INDX_IMPL_CONF       750
#define GSN_CREATE_INDX_IMPL_REF        751
#define GSN_DROP_INDX_IMPL_REQ          752
#define GSN_DROP_INDX_IMPL_CONF         753
#define GSN_DROP_INDX_IMPL_REF          754
#define GSN_ALTER_INDX_IMPL_REQ         755
#define GSN_ALTER_INDX_IMPL_CONF        756
#define GSN_ALTER_INDX_IMPL_REF         757

#define GSN_BUILD_INDX_IMPL_REQ         758
#define GSN_BUILD_INDX_IMPL_CONF        759
#define GSN_BUILD_INDX_IMPL_REF         760

#define GSN_CREATE_NODEGROUP_REQ        144
#define GSN_CREATE_NODEGROUP_REF        145
#define GSN_CREATE_NODEGROUP_CONF       146

#define GSN_CREATE_NODEGROUP_IMPL_REQ   148
#define GSN_CREATE_NODEGROUP_IMPL_REF   149
#define GSN_CREATE_NODEGROUP_IMPL_CONF  150

#define GSN_DROP_NODEGROUP_REQ          451
#define GSN_DROP_NODEGROUP_REF          452
#define GSN_DROP_NODEGROUP_CONF         453

#define GSN_DROP_NODEGROUP_IMPL_REQ     454
#define GSN_DROP_NODEGROUP_IMPL_REF     457
#define GSN_DROP_NODEGROUP_IMPL_CONF    448

#define GSN_DATA_FILE_ORD               706

#define GSN_CALLBACK_REQ                707 /*reserved*/
#define GSN_CALLBACK_CONF               708
#define GSN_CALLBACK_ACK                709

#define GSN_RELEASE_PAGES_REQ           680
#define GSN_RELEASE_PAGES_CONF          681

#define GSN_INDEX_STAT_REQ              650
#define GSN_INDEX_STAT_CONF             651
#define GSN_INDEX_STAT_REF              652
#define GSN_INDEX_STAT_IMPL_REQ         653
#define GSN_INDEX_STAT_IMPL_CONF        654
#define GSN_INDEX_STAT_IMPL_REF         655
#define GSN_INDEX_STAT_REP              656

#define GSN_NODE_STARTED_REP            769

#define GSN_PAUSE_LCP_REQ               770
#define GSN_PAUSE_LCP_CONF              771
#define GSN_FLUSH_LCP_REP_REQ           772
#define GSN_FLUSH_LCP_REP_CONF          773

#define GSN_ISOLATE_ORD                 774

/* 775 free, never used in released version */

#define GSN_ALLOC_NODEID_REP            776
#define GSN_INCL_NODE_HB_PROTOCOL_REP   777
#define GSN_NDBCNTR_START_WAIT_REP      778
#define GSN_NDBCNTR_STARTED_REP         779
#define GSN_SUMA_HANDOVER_COMPLETE_REP  780
#define GSN_END_TOREP                   781
#define GSN_LOCAL_RECOVERY_COMP_REP     782

#define GSN_PROCESSINFO_REP             783
#define GSN_SYNC_PAGE_CACHE_REQ         784
#define GSN_SYNC_PAGE_CACHE_CONF        785

#define GSN_SYNC_EXTENT_PAGES_REQ       786
#define GSN_SYNC_EXTENT_PAGES_CONF      787

#define GSN_RESTORABLE_GCI_REP          788

#define GSN_LCP_START_REP               789 /* No longer used */

#define GSN_WAIT_ALL_COMPLETE_LCP_REQ   790
#define GSN_WAIT_ALL_COMPLETE_LCP_CONF  791

#define GSN_WAIT_COMPLETE_LCP_REQ       792
#define GSN_WAIT_COMPLETE_LCP_CONF      793

#define GSN_INFORM_BACKUP_DROP_TAB_REQ  794
#define GSN_INFORM_BACKUP_DROP_TAB_CONF 795

#define GSN_HALT_COPY_FRAG_REQ          796
#define GSN_HALT_COPY_FRAG_CONF         797
#define GSN_HALT_COPY_FRAG_REF          798

#define GSN_RESUME_COPY_FRAG_REQ        799
#define GSN_RESUME_COPY_FRAG_CONF       800
#define GSN_RESUME_COPY_FRAG_REF        801

#define GSN_READ_LOCAL_SYSFILE_REQ      802
#define GSN_READ_LOCAL_SYSFILE_CONF     803
#define GSN_WRITE_LOCAL_SYSFILE_REQ     804
#define GSN_WRITE_LOCAL_SYSFILE_CONF    805

#define GSN_CUT_UNDO_LOG_TAIL_REQ       806
#define GSN_CUT_UNDO_LOG_TAIL_CONF      807

#define GSN_CUT_REDO_LOG_TAIL_REQ       808
#define GSN_CUT_REDO_LOG_TAIL_CONF      809

#define GSN_LCP_ALL_COMPLETE_REQ        810
#define GSN_LCP_ALL_COMPLETE_CONF       811

#define GSN_START_DISTRIBUTED_LCP_ORD   812
#define GSN_START_FULL_LOCAL_LCP_ORD    813
#define GSN_COPY_FRAG_IN_PROGRESS_REP   814
#define GSN_COPY_FRAG_NOT_IN_PROGRESS_REP 815

#define GSN_SET_LOCAL_LCP_ID_REQ        816
#define GSN_SET_LOCAL_LCP_ID_CONF       817

#define GSN_START_NODE_LCP_REQ          818
#define GSN_START_NODE_LCP_CONF         819

#define GSN_GET_LATEST_GCI_REQ          820

#define GSN_UNDO_LOG_LEVEL_REP          821
#define GSN_START_LOCAL_LCP_ORD         822

#define GSN_INFO_GCP_STOP_TIMER         823

#define GSN_CHECK_LCP_IDLE_ORD          824

#define GSN_SET_LATEST_LCP_ID           825
#define GSN_SYNC_PAGE_WAIT_REP          826

#define GSN_REDO_STATE_REP              827
#endif
