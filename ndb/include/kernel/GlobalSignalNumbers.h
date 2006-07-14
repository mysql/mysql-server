/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#ifndef GLOBAL_SIGNAL_NUMBERS_H
#define GLOBAL_SIGNAL_NUMBERS_H

#include <kernel_types.h>
/**
 * NOTE
 *
 * When adding a new signal, remember to update MAX_GSN and SignalNames.cpp
 */
const GlobalSignalNumber MAX_GSN = 712;

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

/* 40 unused */
/* 41 unused */
/* 42 unused */
/* 43 unused */
/* 44 unused */
/* 45 unused */
/* 46 unused */
/* 47 unused */
/* 48 unused */
/* 49 unused */
/* 50 unused */
/* 51 unused */
/* 52 unused */
/* 53 unused */
/* 54 unused */
/* 55 unused */
/* 56 unused */
/* 57 unused */
/* 58 unused */
/* 59 unused */
/* 60 unused */
/* 61 unused */
/* 62 unused */
/* 63 unused */
/* 64 unused */
/* 65 unused */
/* 66 unused */

/**
 * These are used only by kernel
 */

#define GSN_ACC_ABORTCONF               67
/* 68 unused */
/* 69 unused */
/* 70 unused */
#define GSN_ACC_ABORTREQ                71
#define GSN_ACC_CHECK_SCAN              72
#define GSN_ACC_COMMITCONF              73
#define GSN_ACC_COMMITREQ               74
#define GSN_ACC_CONTOPCONF              75
#define GSN_ACC_CONTOPREQ               76
#define GSN_ACC_LCPCONF                 77
#define GSN_ACC_LCPREF                  78
#define GSN_ACC_LCPREQ                  79
#define GSN_ACC_LCPSTARTED              80
#define GSN_ACC_OVER_REC                81

#define GSN_ACC_SAVE_PAGES              83
#define GSN_ACC_SCAN_INFO               84
#define GSN_ACC_SCAN_INFO24             85
#define GSN_ACC_SCANCONF                86
#define GSN_ACC_SCANREF                 87
#define GSN_ACC_SCANREQ                 88
#define GSN_ACC_SRCONF                  89
#define GSN_ACC_SRREF                   90
#define GSN_ACC_SRREQ                   91
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

#define GSN_API_FAILCONF                113
#define GSN_API_FAILREQ                 114
#define GSN_CNTR_START_REQ              115
/* 116 not unused */
#define GSN_CNTR_START_REF              117
#define GSN_CNTR_START_CONF             118
#define GSN_CNTR_START_REP              119
/* 120 unused */
/* 121 unused */
/* 122 unused */
/* 123 unused */
/* 124 unused */
#define GSN_CHECK_LCP_STOP              125
#define GSN_CLOSE_COMCONF               126 /* local */
#define GSN_CLOSE_COMREQ                127 /* local */
#define GSN_CM_ACKADD                   128 /* distr. */
/* 129 unused */
#define GSN_CM_ADD                      130 /* distr. */
/* 131 unused */
/* 132 not unused */
/* 133 not unused */
#define GSN_CM_HEARTBEAT                134 /* distr. */
/* 135 unused */
/* 136 unused */
/* 137 unused */
#define GSN_CM_NODEINFOCONF             138 /* distr. */
#define GSN_CM_NODEINFOREF              139 /* distr. */
#define GSN_CM_NODEINFOREQ              140 /* distr. */
#define GSN_CM_REGCONF                  141 /* distr. */
#define GSN_CM_REGREF                   142 /* distr. */
#define GSN_CM_REGREQ                   143 /* distr. */
/* 144 unused */
/* 145 unused */
/* 146 unused */
#define GSN_CM_ADD_REP                  147 /* local */
/* 148 unused  */
/* 149 unused  */
/* 150 unused  */
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
#define GSN_COPY_STATECONF              174
#define GSN_COPY_STATEREQ               175
#define GSN_COPY_TABCONF                176
#define GSN_COPY_TABREQ                 177
#define GSN_CREATE_FRAGCONF             178
#define GSN_CREATE_FRAGREF              179
#define GSN_CREATE_FRAGREQ              180
#define GSN_DEBUG_SIG                   181
#define GSN_DI_FCOUNTCONF               182
#define GSN_DI_FCOUNTREF                183
#define GSN_DI_FCOUNTREQ                184
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

#define GSN_WAIT_DROP_TAB_REQ           208
#define GSN_WAIT_DROP_TAB_REF           209
#define GSN_WAIT_DROP_TAB_CONF          216

/*****************************************/

#define GSN_UPDATE_TOCONF               205
#define GSN_UPDATE_TOREF                206
#define GSN_UPDATE_TOREQ                207

#define GSN_DIGETNODESCONF              210
#define GSN_DIGETNODESREF               211
#define GSN_DIGETNODESREQ               212
#define GSN_DIGETPRIMCONF               213
#define GSN_DIGETPRIMREF                214
#define GSN_DIGETPRIMREQ                215

#define GSN_DIH_RESTARTCONF             217
#define GSN_DIH_RESTARTREF              218
#define GSN_DIH_RESTARTREQ              219

/* 220 not unused */
/* 221 not unused */
/* 222 not unused */

#define GSN_EMPTY_LCP_REQ               223
#define GSN_EMPTY_LCP_CONF              224

#define GSN_SCHEMA_INFO                 225
#define GSN_SCHEMA_INFOCONF             226

#define GSN_MASTER_GCPCONF              227
#define GSN_MASTER_GCPREF               228
#define GSN_MASTER_GCPREQ               229

/* 230 not unused */
/* 231 not unused */

#define GSN_DIRELEASECONF               232
#define GSN_DIRELEASEREF                233
#define GSN_DIRELEASEREQ                234
#define GSN_DISCONNECT_REP              235
#define GSN_DISEIZECONF                 236
#define GSN_DISEIZEREF                  237
#define GSN_DISEIZEREQ                  238
#define GSN_DIVERIFYCONF                239
#define GSN_DIVERIFYREF                 240
#define GSN_DIVERIFYREQ                 241
#define GSN_ENABLE_COMORD               242
#define GSN_END_LCPCONF                 243
#define GSN_END_LCPREQ                  244
#define GSN_END_TOCONF                  245
#define GSN_END_TOREQ                   246
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
#define GSN_SR_FRAGIDCONF               285
#define GSN_SR_FRAGIDREF                286
#define GSN_SR_FRAGIDREQ                287
#define GSN_GETGCICONF                  288
#define GSN_GETGCIREQ                   289
#define GSN_HOT_SPAREREP                290
#define GSN_INCL_NODECONF               291
#define GSN_INCL_NODEREF                292
#define GSN_INCL_NODEREQ                293
#define GSN_LCP_FRAGIDCONF              294
#define GSN_LCP_FRAGIDREF               295
#define GSN_LCP_FRAGIDREQ               296
#define GSN_LCP_HOLDOPCONF              297
#define GSN_LCP_HOLDOPREF               298
#define GSN_LCP_HOLDOPREQ               299
#define GSN_SHRINKCHECK2                301
#define GSN_GET_SCHEMA_INFOREQ          302
/* 303 not unused */
/* 304 not unused */
#define GSN_LQH_RESTART_OP              305
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
#define GSN_NEXTOPERATION               333

#define GSN_READ_CONFIG_REQ             334 /* new name for sizealt, local */
#define GSN_READ_CONFIG_CONF            335 /* new name for sizealt, local */

/* 336 unused */
/* 337 unused */
/* 338 unused */
#define GSN_OPEN_COMCONF                339
#define GSN_OPEN_COMREF                 340
#define GSN_OPEN_COMREQ                 341
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

#define GSN_LQH_ALLOCREQ                359
#define GSN_TUP_ALLOCREQ                360
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
#define GSN_START_RECCONF               384
#define GSN_START_RECREF                385
#define GSN_START_RECREQ                386
#define GSN_START_TOCONF                387
#define GSN_START_TOREQ                 388
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
#define GSN_TUP_LCPCONF                 421
#define GSN_TUP_LCPREF                  422
#define GSN_TUP_LCPREQ                  423
#define GSN_TUP_LCPSTARTED              424
#define GSN_TUP_PREPLCPCONF             425
#define GSN_TUP_PREPLCPREF              426
#define GSN_TUP_PREPLCPREQ              427
#define GSN_TUP_SRCONF                  428
#define GSN_TUP_SRREF                   429
#define GSN_TUP_SRREQ                   430
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

#define GSN_STATISTICS_REQ              448
#define GSN_STOP_ORD                    449
#define GSN_TAMPER_ORD                  450
#define GSN_SET_VAR_REQ                 451
#define GSN_SET_VAR_CONF                452
#define GSN_SET_VAR_REF                 453
#define GSN_STATISTICS_CONF             454

#define GSN_START_ORD                   455
/* 457 unused */

#define GSN_EVENT_SUBSCRIBE_REQ         458
#define GSN_EVENT_SUBSCRIBE_CONF        459
#define GSN_EVENT_SUBSCRIBE_REF         460
#define GSN_ACC_COM_BLOCK               461
#define GSN_ACC_COM_UNBLOCK             462
#define GSN_TUP_COM_BLOCK               463
#define GSN_TUP_COM_UNBLOCK             464

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
/*
#define GSN_SUB_START_REQ               579
#define GSN_SUB_START_REF               580
#define GSN_SUB_START_CONF              581
*/
#define GSN_SUB_SYNC_REQ                582
#define GSN_SUB_SYNC_REF                583
#define GSN_SUB_SYNC_CONF               584
#define GSN_SUB_META_DATA               585
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

/**
 * Grep signals
 */
#define GSN_ALTER_TABLE_REP             606
#define GSN_API_BROADCAST_REP           607
#define GSN_GREP_SUB_CREATE_CONF        608
#define GSN_GREP_CREATE_REQ             609
#define GSN_GREP_CREATE_REF             610
#define GSN_GREP_CREATE_CONF            611

#define GSN_GREP_SUB_START_REQ          612
#define GSN_GREP_SUB_START_REF          613
#define GSN_GREP_SUB_START_CONF         614
#define GSN_GREP_START_REQ              615
#define GSN_GREP_START_REF              616
#define GSN_GREP_START_CONF             617

#define GSN_GREP_SUB_SYNC_REQ	        618
#define GSN_GREP_SUB_SYNC_REF	        619
#define GSN_GREP_SUB_SYNC_CONF          620
#define GSN_GREP_SYNC_REQ               621
#define GSN_GREP_SYNC_REF               622
#define GSN_GREP_SYNC_CONF              623

/**
 * REP signals
 */
#define GSN_REP_WAITGCP_REQ             627
#define GSN_REP_WAITGCP_REF             628
#define GSN_REP_WAITGCP_CONF            629
#define GSN_GREP_WAITGCP_REQ            630
#define GSN_GREP_WAITGCP_REF            631
#define GSN_GREP_WAITGCP_CONF           632
#define GSN_REP_GET_GCI_REQ		633
#define GSN_REP_GET_GCI_REF		634
#define GSN_REP_GET_GCI_CONF		635
#define GSN_REP_GET_GCIBUFFER_REQ      	636
#define GSN_REP_GET_GCIBUFFER_REF      	637
#define GSN_REP_GET_GCIBUFFER_CONF     	638
#define GSN_REP_INSERT_GCIBUFFER_REQ  	639
#define GSN_REP_INSERT_GCIBUFFER_REF    640
#define GSN_REP_INSERT_GCIBUFFER_CONF	641
#define GSN_REP_CLEAR_PS_GCIBUFFER_REQ  642
#define GSN_REP_CLEAR_PS_GCIBUFFER_REF  643
#define GSN_REP_CLEAR_PS_GCIBUFFER_CONF 644
#define GSN_REP_CLEAR_SS_GCIBUFFER_REQ  645
#define GSN_REP_CLEAR_SS_GCIBUFFER_REF  646
#define GSN_REP_CLEAR_SS_GCIBUFFER_CONF 647
#define GSN_REP_DATA_PAGE	  	648
#define GSN_REP_GCIBUFFER_ACC_REP 	649

#define GSN_GREP_SUB_REMOVE_REQ	        650
#define GSN_GREP_SUB_REMOVE_REF	        651
#define GSN_GREP_SUB_REMOVE_CONF        652
#define GSN_GREP_REMOVE_REQ             653
#define GSN_GREP_REMOVE_REF             654
#define GSN_GREP_REMOVE_CONF            655

/* Start Global Replication */
#define GSN_GREP_REQ                    656

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

/* GREP */
#define GSN_GREP_CREATE_SUBID_REQ       664      
#define GSN_GREP_CREATE_SUBID_REF       665
#define GSN_GREP_CREATE_SUBID_CONF      666    
#define GSN_REP_DROP_TABLE_REQ          667      
#define GSN_REP_DROP_TABLE_REF          668
#define GSN_REP_DROP_TABLE_CONF         669    

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

/* not used                             680 */
/* not used                             681 */

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
/*
#define GSN_SUMA_START_ME               691
#define GSN_SUMA_HANDOVER_REQ           692
#define GSN_SUMA_HANDOVER_CONF          693
*/
/* not used                             694 */
/* not used                             695 */
/* not used                             696 */

/**
 * GREP restart protocol
 */
#define GSN_GREP_START_ME              706
#define GSN_GREP_ADD_SUB_REQ           707
#define GSN_GREP_ADD_SUB_REF           708
#define GSN_GREP_ADD_SUB_CONF          709


/*
 * EVENT Signals
 */
/*
#define GSN_SUB_GCP_COMPLETE_ACC        699

#define GSN_CREATE_EVNT_REQ             700
#define GSN_CREATE_EVNT_CONF            701
#define GSN_CREATE_EVNT_REF             702

#define GSN_DROP_EVNT_REQ               703
#define GSN_DROP_EVNT_CONF              704
#define GSN_DROP_EVNT_REF               705
*/
#define GSN_TUX_BOUND_INFO		710

#define GSN_ACC_LOCKREQ			711
#define GSN_READ_PSUEDO_REQ             712

/* DICT LOCK signals */
#define GSN_DICT_LOCK_REQ               410
#define GSN_DICT_LOCK_CONF              411
#define GSN_DICT_LOCK_REF               412
#define GSN_DICT_UNLOCK_ORD             420

#endif
