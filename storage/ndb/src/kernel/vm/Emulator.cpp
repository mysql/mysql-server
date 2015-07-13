/*
   Copyright (c) 2003, 2015, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include <ndb_global.h>

#include "Emulator.hpp"
#include <FastScheduler.hpp>
#include <SignalLoggerManager.hpp>
#include <TransporterRegistry.hpp>
#include <TimeQueue.hpp>

#include "Configuration.hpp"
#include "WatchDog.hpp"
#include "ThreadConfig.hpp"
#include "SimBlockList.hpp"

#include <NodeState.hpp>
#include "ndbd_malloc_impl.hpp"

#include <NdbMem.h>
#include <NdbMutex.h>

#include <EventLogger.hpp>
#include <string.h>

#define JAM_FILE_ID 329

extern EventLogger * g_eventLogger;

/**
 * Declare the global variables 
 */

#ifndef NO_EMULATED_JAM
/*
  This is the jam buffer used for non-threaded ndbd (but present also
  in threaded ndbd to allow sharing of object files among the two
  binaries).
 */
EmulatedJamBuffer theEmulatedJamBuffer;
#endif

   GlobalData globalData;

   TimeQueue globalTimeQueue;
   FastScheduler globalScheduler;
   extern TransporterRegistry globalTransporterRegistry;

#ifdef VM_TRACE
   SignalLoggerManager globalSignalLoggers;
#endif

EmulatorData globalEmulatorData;
NdbMutex * theShutdownMutex = 0;

/**
 * DESCRIPTION:
 * 
 * This table maps from JAM_FILE_ID to the corresponding source file name.
 *
 * Each source file that has jam() calls must have a macro definition of the
 * following type:
 *
 *      #define JAM_FILE_ID <number>
 *
 * where <number> is unique to that source file. This definition should be
 * placed *after* the last #include of any file that may directly or indirectly 
 * contain another definition of JAM_FILE_ID, and *before* the first jam() call.
 * 
 * Each include file should also have an '#undef JAM_FILE_ID' at the end of the
 * file. Note that some .cpp files (e.g. SimulatedBlock.cpp) are also used as
 * include files.
 *
 * JAM_FILE_ID is used as an index into the jamFileNames table, so the 
 * corresponding table entry should contain the base name of the source file.
 *
 * MAINTENANCE:
 *
 * If you wish to delete a source file, set the corresponding table entry to 
 * NULL.
 * 
 * If you wish to add jam trace to a new source file, find the first NULL
 * table entry and set it to the base name of the source file. If there is no
 * NULL entry, add a new entry at the end of the table. Add a JAM_FILE_ID 
 * definition to the source file, as described above.
 *
 * To rename a source file, simply update the table entry with the new base 
 * name.
 *
 * To check for duplicate JAM_FILE_ID values, run the two commands below and 
 * check that you get the same line count in both cases.
 *
 * find storage/ndb  -name '*.?pp' -exec egrep '^ *# *define *JAM_FILE_ID' {} \;|sed -e 's/ \+//g'|sort|uniq|wc -l
 * find storage/ndb  -name '*.?pp' -exec egrep '^ *# *define *JAM_FILE_ID' {} \;|sed -e 's/ \+//g'|wc -l
 *
 * TROUBLESHOOTING:
 *
 * Symptom: Compiler error stating that JAM_FILE_ID is undefined.
 * Causes: 
 *   - The source file misses a JAM_FILE_ID definition.
 *   - The first jam() call comes before the JAM_FILE_ID definition.
 *   - JAM_FILE_ID is #undef'ed by an include file included after the definition
 * 
 * Symptom: Compiler warning about JAM_FILE_ID being redefined.
 * Causes:
 *   - Missing #undef at the end of include file.
 *   - File included after the JAM_FILE_ID definition.
 *
 * Symptom: Assert failure for for JamEvent::verifyId() or jam trace entries
 *   pointing to the wrong source file.
 * Cause: jamFileNames[JAM_FILE_ID] does not contain the base name of the source
 *   file.
*/
static const char* const jamFileNames[] =
   {
   "NodeInfo.hpp",                       // 0
   "NodeState.hpp",                      // 1
   "NodeBitmask.hpp",                    // 2
   "LogLevel.hpp",                       // 3
   "AttributeList.hpp",                  // 4
   "AttributeDescriptor.hpp",            // 5
   "AttributeHeader.hpp",                // 6
   "ConfigChange.hpp",                   // 7
   "CreateIndx.hpp",                     // 8
   "StartInfo.hpp",                      // 9
   "GetTableId.hpp",                     // 10
   "NextScan.hpp",                       // 11
   "DihFragCount.hpp",                   // 12
   "CmInit.hpp",                         // 13
   "DropTabFile.hpp",                    // 14
   "BuildIndx.hpp",                      // 15
   "TcContinueB.hpp",                    // 16
   "MasterGCP.hpp",                      // 17
   "UtilPrepare.hpp",                    // 18
   "DictSizeAltReq.hpp",                 // 19
   "TabCommit.hpp",                      // 20
   "LqhTransConf.hpp",                   // 21
   "CallbackSignal.hpp",                 // 22
   "ArbitSignalData.hpp",                // 23
   "FailRep.hpp",                        // 24
   "DropObj.hpp",                        // 25
   "AllocNodeId.hpp",                    // 26
   "LqhKey.hpp",                         // 27
   "CreateNodegroup.hpp",                // 28
   "GetTabInfo.hpp",                     // 29
   "BuildIndxImpl.hpp",                  // 30
   "Sync.hpp",                           // 31
   "CntrMasterReq.hpp",                  // 32
   "CreateIndxImpl.hpp",                 // 33
   "UtilLock.hpp",                       // 34
   "ApiVersion.hpp",                     // 35
   "CreateNodegroupImpl.hpp",            // 36
   "DihAddFrag.hpp",                     // 37
   "LqhTransReq.hpp",                    // 38
   "DataFileOrd.hpp",                    // 39
   "EnableCom.hpp",                      // 40
   "SignalDataPrint.hpp",                // 41
   "SignalDroppedRep.hpp",               // 42
   "ApiBroadcast.hpp",                   // 43
   "LqhFrag.hpp",                        // 44
   "CopyFrag.hpp",                       // 45
   "CreateTab.hpp",                      // 46
   "BackupContinueB.hpp",                // 47
   "MasterLCP.hpp",                      // 48
   "WaitGCP.hpp",                        // 49
   "LocalRouteOrd.hpp",                  // 50
   "StopMe.hpp",                         // 51
   "EventReport.hpp",                    // 52
   "CreateFilegroupImpl.hpp",            // 53
   "LgmanContinueB.hpp",                 // 54
   "ListTables.hpp",                     // 55
   "ScanTab.hpp",                        // 56
   "TupKey.hpp",                         // 57
   "TcKeyConf.hpp",                      // 58
   "NodeFailRep.hpp",                    // 59
   "RouteOrd.hpp",                       // 60
   "SignalData.hpp",                     // 61
   "FsRemoveReq.hpp",                    // 62
   "DropIndxImpl.hpp",                   // 63
   "CreateHashMap.hpp",                  // 64
   "CmRegSignalData.hpp",                // 65
   "LqhSizeAltReq.hpp",                  // 66
   "StartFragReq.hpp",                   // 67
   "DictTakeover.hpp",                   // 68
   "FireTrigOrd.hpp",                    // 69
   "BuildFK.hpp",                        // 70
   "DropTrig.hpp",                       // 71
   "AlterTab.hpp",                       // 72
   "PackedSignal.hpp",                   // 73
   "DropNodegroup.hpp",                  // 74
   "ReadConfig.hpp",                     // 75
   "InvalidateNodeLCPReq.hpp",           // 76
   "ApiRegSignalData.hpp",               // 77
   "BackupImpl.hpp",                     // 78
   "SumaImpl.hpp",                       // 79
   "CreateFragmentation.hpp",            // 80
   "AlterIndx.hpp",                      // 81
   "BackupLockTab.hpp",                  // 82
   "DihGetTabInfo.hpp",                  // 83
   "DihRestart.hpp",                     // 84
   "TupCommit.hpp",                      // 85
   "Extent.hpp",                         // 86
   "DictTabInfo.hpp",                    // 87
   "CmvmiCfgConf.hpp",                   // 88
   "BlockCommitOrd.hpp",                 // 89
   "DiGetNodes.hpp",                     // 90
   "Upgrade.hpp",                        // 91
   "ExecFragReq.hpp",                    // 92
   "TcHbRep.hpp",                        // 93
   "TcKeyFailConf.hpp",                  // 94
   "TuxMaint.hpp",                       // 95
   "DihStartTab.hpp",                    // 96
   "GetConfig.hpp",                      // 97
   "CreateFilegroup.hpp",                // 98
   "ReleasePages.hpp",                   // 99
   "CreateTrig.hpp",                     // 100
   "BackupSignalData.hpp",               // 101
   "TuxSizeAltReq.hpp",                  // 102
   "CreateEvnt.hpp",                     // 103
   "CreateTrigImpl.hpp",                 // 104
   "StartRec.hpp",                       // 105
   "ContinueFragmented.hpp",             // 106
   "CreateObj.hpp",                      // 107
   "DihScanTab.hpp",                     // 108
   "AccSizeAltReq.hpp",                  // 109
   "DropFK.hpp",                         // 110
   "HotSpareRep.hpp",                    // 111
   "AlterTable.hpp",                     // 112
   "DisconnectRep.hpp",                  // 113
   "DihContinueB.hpp",                   // 114
   "TupSizeAltReq.hpp",                  // 115
   "AllocMem.hpp",                       // 116
   "TamperOrd.hpp",                      // 117
   "ResumeReq.hpp",                      // 118
   "UtilRelease.hpp",                    // 119
   "DropFKImpl.hpp",                     // 120
   "AccScan.hpp",                        // 121
   "DbinfoScan.hpp",                     // 122
   "SchemaTrans.hpp",                    // 123
   "UtilDelete.hpp",                     // 124
   "TcSizeAltReq.hpp",                   // 125
   "DictStart.hpp",                      // 126
   "TcKeyReq.hpp",                       // 127
   "SrFragidConf.hpp",                   // 128
   "QueryTree.hpp",                      // 129
   "NdbfsContinueB.hpp",                 // 130
   "GCP.hpp",                            // 131
   "TcRollbackRep.hpp",                  // 132
   "DictLock.hpp",                       // 133
   "ScanFrag.hpp",                       // 134
   "DropFilegroup.hpp",                  // 135
   "FsAppendReq.hpp",                    // 136
   "DumpStateOrd.hpp",                   // 137
   "DropTab.hpp",                        // 138
   "DictSchemaInfo.hpp",                 // 139
   "RestoreContinueB.hpp",               // 140
   "AbortAll.hpp",                       // 141
   "NdbSttor.hpp",                       // 142
   "DictObjOp.hpp",                      // 143
   "StopPerm.hpp",                       // 144
   "UtilExecute.hpp",                    // 145
   "ConfigParamId.hpp",                  // 146
   "DropIndx.hpp",                       // 147
   "FsOpenReq.hpp",                      // 148
   "DropFilegroupImpl.hpp",              // 149
   "NFCompleteRep.hpp",                  // 150
   "CreateTable.hpp",                    // 151
   "StartMe.hpp",                        // 152
   "AccLock.hpp",                        // 153
   "CntrMasterConf.hpp",                 // 154
   "DbspjErr.hpp",                       // 155
   "FsReadWriteReq.hpp",                 // 156
   "EmptyLcp.hpp",                       // 157
   "DropNodegroupImpl.hpp",              // 158
   "InvalidateNodeLCPConf.hpp",          // 159
   "PrepFailReqRef.hpp",                 // 160
   "PrepDropTab.hpp",                    // 161
   "KeyInfo.hpp",                        // 162
   "TcCommit.hpp",                       // 163
   "TakeOver.hpp",                       // 164
   "NodeStateSignalData.hpp",            // 165
   "AccFrag.hpp",                        // 166
   "DropTrigImpl.hpp",                   // 167
   "IndxAttrInfo.hpp",                   // 168
   "TuxBound.hpp",                       // 169
   "LCP.hpp",                            // 170
   "StopForCrash.hpp",                   // 171
   "DihSwitchReplica.hpp",               // 172
   "CreateFK.hpp",                       // 173
   "CloseComReqConf.hpp",                // 174
   "CopyActive.hpp",                     // 175
   "DropTable.hpp",                      // 176
   "TcKeyRef.hpp",                       // 177
   "TuxContinueB.hpp",                   // 178
   "PgmanContinueB.hpp",                 // 179
   "SystemError.hpp",                    // 180
   "DihSizeAltReq.hpp",                  // 181
   "TsmanContinueB.hpp",                 // 182
   "SetVarReq.hpp",                      // 183
   "StartOrd.hpp",                       // 184
   "AttrInfo.hpp",                       // 185
   "UtilSequence.hpp",                   // 186
   "DictSignal.hpp",                     // 187
   "StopReq.hpp",                        // 188
   "TrigAttrInfo.hpp",                   // 189
   "CheckNodeGroups.hpp",                // 190
   "CntrStart.hpp",                      // 191
   "TransIdAI.hpp",                      // 192
   "IndexStatSignal.hpp",                // 193
   "FsRef.hpp",                          // 194
   "SetLogLevelOrd.hpp",                 // 195
   "TestOrd.hpp",                        // 196
   "TupFrag.hpp",                        // 197
   "RelTabMem.hpp",                      // 198
   "ReadNodesConf.hpp",                  // 199
   "HashMapImpl.hpp",                    // 200
   "CopyData.hpp",                       // 201
   "FsCloseReq.hpp",                     // 202
   "IndxKeyInfo.hpp",                    // 203
   "StartPerm.hpp",                      // 204
   "SchemaTransImpl.hpp",                // 205
   "FsConf.hpp",                         // 206
   "BuildFKImpl.hpp",                    // 207
   "CreateFKImpl.hpp",                   // 208
   "AlterIndxImpl.hpp",                  // 209
   "DiAddTab.hpp",                       // 210
   "TcIndx.hpp",                         // 211
   "CopyGCIReq.hpp",                     // 212
   "NodePing.hpp",                       // 213
   "RestoreImpl.hpp",                    // 214
   "Interpreter.hpp",                    // 215
   "statedesc.hpp",                      // 216
   "RefConvert.hpp",                     // 217
   "ndbd.hpp",                           // 218
   "SectionReader.hpp",                  // 219
   "SafeMutex.hpp",                      // 220
   "KeyTable.hpp",                       // 221
   "dummy_nonmt.cpp",                    // 222
   "Callback.hpp",                       // 223
   "SimplePropertiesSection.cpp",        // 224
   "test.cpp",                           // 225
   "TransporterCallback.cpp",            // 226
   "Array.hpp",                          // 227
   "LongSignalImpl.hpp",                 // 228
   "dummy_mt.cpp",                       // 229
   "Ndbinfo.hpp",                        // 230
   "ThreadConfig.hpp",                   // 231
   "SimplePropertiesSection_nonmt.cpp",  // 232
   "DynArr256.cpp",                      // 233
   "ndbd_malloc.hpp",                    // 234
   "WatchDog.cpp",                       // 235
   "mt.cpp",                             // 236
   "testLongSig.cpp",                    // 237
   "TransporterCallback_mt.cpp",         // 238
   "NdbinfoTables.cpp",                  // 239
   "SuperPool.cpp",                      // 240
   "LongSignal_nonmt.cpp",               // 241
   "FastScheduler.cpp",                  // 242
   "TransporterCallback_nonmt.cpp",      // 243
   "FastScheduler.hpp",                  // 244
   "CountingSemaphore.hpp",              // 245
   "NdbdSuperPool.cpp",                  // 246
   "TimeQueue.hpp",                      // 247
   "SimulatedBlock.hpp",                 // 248
   "IntrusiveList.cpp",                  // 249
   "test_context.cpp",                   // 250
   "NdbSeqLock.hpp",                     // 251
   "SimulatedBlock.cpp",                 // 252
   "WatchDog.hpp",                       // 253
   "SimplePropertiesSection_mt.cpp",     // 254
   "Pool.cpp",                           // 255
   "ClusterConfiguration.cpp",           // 256
   "DLCHashTable.hpp",                   // 257
   "KeyTable2.hpp",                      // 258
   "KeyDescriptor.hpp",                  // 259
   "Emulator.hpp",                       // 260
   "LHLevel.hpp",                        // 261
   "LongSignal.cpp",                     // 262
   "ThreadConfig.cpp",                   // 263
   "LinearPool.hpp",                     // 264
   "SafeMutex.cpp",                      // 265
   "SafeCounter.cpp",                    // 266
   "bench_pool.cpp",                     // 267
   "DataBuffer2.hpp",                    // 268
   "Mutex.hpp",                          // 269
   "testSuperPool.cpp",                  // 270
   "CArray.hpp",                         // 271
   "mt_thr_config.hpp",                  // 272
   "TimeQueue.cpp",                      // 273
   "DataBuffer.hpp",                     // 274
   "mt.hpp",                             // 275
   "Configuration.hpp",                  // 276
   "GlobalData.hpp",                     // 277
   "RWPool.cpp",                         // 278
   "GlobalData.cpp",                     // 279
   "Prio.hpp",                           // 280
   "SuperPool.hpp",                      // 281
   "pc.hpp",                             // 282
   "LockQueue.hpp",                      // 283
   "ClusterConfiguration.hpp",           // 284
   "SimulatedBlock_nonmt.cpp",           // 285
   "SafeCounter.hpp",                    // 286
   "ndbd_malloc.cpp",                    // 287
   "LongSignal.hpp",                     // 288
   "ArenaPool.hpp",                      // 289
   "testDataBuffer.cpp",                 // 290
   "ndbd_malloc_impl.hpp",               // 291
   "ArrayPool.hpp",                      // 292
   "Mutex.cpp",                          // 293
   "WOPool.cpp",                         // 294
   "test_context.hpp",                   // 295
   "ndbd_malloc_impl.cpp",               // 296
   "mt_thr_config.cpp",                  // 297
   "IntrusiveList.hpp",                  // 298
   "DynArr256.hpp",                      // 299
   "LongSignal_mt.cpp",                  // 300
   "Configuration.cpp",                  // 301
   "WaitQueue.hpp",                      // 302
   "WOPool.hpp",                         // 303
   "CountingPool.cpp",                   // 304
   "TransporterCallbackKernel.hpp",      // 305
   "NdbdSuperPool.hpp",                  // 306
   "DLHashTable2.hpp",                   // 307
   "VMSignal.cpp",                       // 308
   "ArenaPool.cpp",                      // 309
   "LHLevel.cpp",                        // 310
   "RWPool.hpp",                         // 311
   "t.cpp",                              // 312
   "DLHashTable.hpp",                    // 313
   "VMSignal.hpp",                       // 314
   "Pool.hpp",                           // 315
   "Rope.hpp",                           // 316
   "KeyTable2Ref.hpp",                   // 317
   "LockQueue.cpp",                      // 318
   "arrayListTest.cpp",                  // 319
   "main.cpp",                           // 320
   "arrayPoolTest.cpp",                  // 321
   "SimBlockList.hpp",                   // 322
   "lock.hpp",                           // 323
   "rr.cpp",                             // 324
   "testCopy.cpp",                       // 325
   "Ndbinfo.cpp",                        // 326
   "SectionReader.cpp",                  // 327
   "RequestTracker.hpp",                 // 328
   "Emulator.cpp",                       // 329
   "Rope.cpp",                           // 330
   "SimulatedBlock_mt.cpp",              // 331
   "CountingPool.hpp",                   // 332
   "angel.cpp",                          // 333
   "trpman.hpp",                         // 334
   "pgman.cpp",                          // 335
   "RestoreProxy.cpp",                   // 336
   "DbgdmProxy.hpp",                     // 337
   "DbgdmProxy.cpp",                     // 338
   "lgman.hpp",                          // 339
   "thrman.hpp",                         // 340
   "DbaccProxy.cpp",                     // 341
   "Container.hpp",                      // 342
   "DbaccProxy.hpp",                     // 343
   "Dbacc.hpp",                          // 344
   "DbaccMain.cpp",                      // 345
   "DbaccInit.cpp",                      // 346
   "record_types.hpp",                   // 347
   "DbtcProxy.hpp",                      // 348
   "DbtcInit.cpp",                       // 349
   "Dbtc.hpp",                           // 350
   "DbtcStateDesc.cpp",                  // 351
   "DbtcProxy.cpp",                      // 352
   "DbtcMain.cpp",                       // 353
   "DbdihMain.cpp",                      // 354
   "DbdihInit.cpp",                      // 355
   "Dbdih.hpp",                          // 356
   "Sysfile.hpp",                        // 357
   "printSysfile.cpp",                   // 358
   "tsman.cpp",                          // 359
   "QmgrMain.cpp",                       // 360
   "QmgrInit.cpp",                       // 361
   "Qmgr.hpp",                           // 362
   "timer.hpp",                          // 363
   "diskpage.cpp",                       // 364
   "DbtuxGen.cpp",                       // 365
   "DbtuxDebug.cpp",                     // 366
   "DbtuxStat.cpp",                      // 367
   "DbtuxSearch.cpp",                    // 368
   "DbtuxMaint.cpp",                     // 369
   "DbtuxProxy.cpp",                     // 370
   "DbtuxScan.cpp",                      // 371
   "DbtuxNode.cpp",                      // 372
   "DbtuxBuild.cpp",                     // 373
   "Dbtux.hpp",                          // 374
   "DbtuxTree.cpp",                      // 375
   "DbtuxProxy.hpp",                     // 376
   "DbtuxMeta.cpp",                      // 377
   "DbtuxCmp.cpp",                       // 378
   "Cmvmi.hpp",                          // 379
   "Cmvmi.cpp",                          // 380
   "AsyncIoThread.hpp",                  // 381
   "MemoryChannelTest.cpp",              // 382
   "Filename.cpp",                       // 383
   "PosixAsyncFile.cpp",                 // 384
   "Ndbfs.hpp",                          // 385
   "OpenFiles.hpp",                      // 386
   "AsyncFile.cpp",                      // 387
   "AsyncIoThread.cpp",                  // 388
   "AsyncFileTest.cpp",                  // 389
   "MemoryChannel.cpp",                  // 390
   "AsyncFile.hpp",                      // 391
   "Filename.hpp",                       // 392
   "Ndbfs.cpp",                          // 393
   "VoidFs.cpp",                         // 394
   "Win32AsyncFile.hpp",                 // 395
   "MemoryChannel.hpp",                  // 396
   "PosixAsyncFile.hpp",                 // 397
   "Pool.hpp",                           // 398
   "Win32AsyncFile.cpp",                 // 399
   "DbUtil.cpp",                         // 400
   "DbUtil.hpp",                         // 401
   "DbtupRoutines.cpp",                  // 402
   "DbtupProxy.hpp",                     // 403
   "Undo_buffer.hpp",                    // 404
   "DbtupVarAlloc.cpp",                  // 405
   "DbtupStoredProcDef.cpp",             // 406
   "DbtupPagMan.cpp",                    // 407
   "DbtupScan.cpp",                      // 408
   "DbtupAbort.cpp",                     // 409
   "DbtupBuffer.cpp",                    // 410
   "DbtupDebug.cpp",                     // 411
   "DbtupTabDesMan.cpp",                 // 412
   "DbtupProxy.cpp",                     // 413
   "Dbtup.hpp",                          // 414
   "DbtupPageMap.cpp",                   // 415
   "DbtupCommit.cpp",                    // 416
   "DbtupClient.cpp",                    // 417
   "DbtupIndex.cpp",                     // 418
   "tuppage.hpp",                        // 419
   "DbtupGen.cpp",                       // 420
   "DbtupFixAlloc.cpp",                  // 421
   "DbtupExecQuery.cpp",                 // 422
   "DbtupTrigger.cpp",                   // 423
   "DbtupMeta.cpp",                      // 424
   "AttributeOffset.hpp",                // 425
   "DbtupDiskAlloc.cpp",                 // 426
   "tuppage.cpp",                        // 427
   "test_varpage.cpp",                   // 428
   "Undo_buffer.cpp",                    // 429
   "trpman.cpp",                         // 430
   "print_file.cpp",                     // 431
   "Trix.hpp",                           // 432
   "Trix.cpp",                           // 433
   "PgmanProxy.hpp",                     // 434
   "RestoreProxy.hpp",                   // 435
   "diskpage.hpp",                       // 436
   "LocalProxy.cpp",                     // 437
   "LocalProxy.hpp",                     // 438
   "restore.hpp",                        // 439
   "thrman.cpp",                         // 440
   "lgman.cpp",                          // 441
   "DblqhProxy.cpp",                     // 442
   "DblqhCommon.hpp",                    // 443
   "DblqhCommon.cpp",                    // 444
   "DblqhProxy.hpp",                     // 445
   "DblqhStateDesc.cpp",                 // 446
   "records.hpp",                        // 447
   "records.cpp",                        // 448
   "reader.cpp",                         // 449
   "Dblqh.hpp",                          // 450
   "DblqhMain.cpp",                      // 451
   "DblqhInit.cpp",                      // 452
   "restore.cpp",                        // 453
   "Dbinfo.hpp",                         // 454
   "Dbinfo.cpp",                         // 455
   "tsman.hpp",                          // 456
   "Ndbcntr.hpp",                        // 457
   "NdbcntrMain.cpp",                    // 458
   "NdbcntrInit.cpp",                    // 459
   "NdbcntrSysTable.cpp",                // 460
   "mutexes.hpp",                        // 461
   "pgman.hpp",                          // 462
   "printSchemaFile.cpp",                // 463
   "Dbdict.hpp",                         // 464
   "Dbdict.cpp",                         // 465
   "SchemaFile.hpp",                     // 466
   "Suma.cpp",                           // 467
   "SumaInit.cpp",                       // 468
   "Suma.hpp",                           // 469
   "PgmanProxy.cpp",                     // 470
   "BackupProxy.cpp",                    // 471
   "BackupInit.cpp",                     // 472
   "BackupFormat.hpp",                   // 473
   "Backup.hpp",                         // 474
   "Backup.cpp",                         // 475
   "read.cpp",                           // 476
   "FsBuffer.hpp",                       // 477
   "BackupProxy.hpp",                    // 478
   "DbspjMain.cpp",                      // 479
   "DbspjProxy.hpp",                     // 480
   "Dbspj.hpp",                          // 481
   "DbspjInit.cpp",                      // 482
   "DbspjProxy.cpp",                     // 483
   "ndbd.cpp",                           // 484
   "main.cpp",                           // 485
   "TimeModule.hpp",                     // 486
   "ErrorReporter.hpp",                  // 487
   "TimeModule.cpp",                     // 488
   "ErrorHandlingMacros.hpp",            // 489
   "ErrorReporter.cpp",                  // 490
   "angel.hpp",                          // 491
   "SimBlockList.cpp",                   // 492
   "CopyTab.cpp"                         // 493
   "IsolateOrd.hpp",                     // 494
   "IsolateOrd.cpp"                      // 495
   "SegmentList.hpp",                    // 496
   "SegmentList.cpp"                     // 497
   };

bool 
JamEvent::verifyId(Uint32 fileId, const char* pathName)
{
  if (fileId >= sizeof jamFileNames/sizeof jamFileNames[0])
  {
    return false;
  }
  else if (jamFileNames[fileId] == NULL)
  {
    return false;
  }
  else
  {
    if (false)
    {
      ndbout << "JamEvent::verifyId() fileId=" << fileId <<
        " pathName=" << pathName <<
        " jamFileNames[fileId]=" << jamFileNames[fileId] << endl;
    }
    /** 
     * Check if pathName ends with jamFileNames[fileId]. Observe that the
     * basename() libc function is neither thread safe nor univeraslly 
     * portable, therefore it is not used here.
     */
    const size_t pathLen = strlen(pathName);
    const size_t baseLen = strlen(jamFileNames[fileId]);

    /*
      With Visual C++, __FILE__ is always in lowercase. Therefore we must use
      strcasecmp() rather than strcmp(), since jamFileNames contains mixed-case
      names.
    */
    return pathLen >= baseLen &&
      native_strcasecmp(pathName+pathLen-baseLen, jamFileNames[fileId]) == 0;
  }
}


const char* JamEvent::getFileName() const
{
  if (getFileId() < sizeof jamFileNames/sizeof jamFileNames[0])
  {
    return jamFileNames[getFileId()];
  }
  else
  {
    return NULL;
  }
}

EmulatorData::EmulatorData(){
  theConfiguration = 0;
  theWatchDog      = 0;
  theThreadConfig  = 0;
  theSimBlockList  = 0;
  theShutdownMutex = 0;
  m_socket_server = 0;
  m_mem_manager = 0;
}

void
EmulatorData::create(){
  /*
    Global jam() buffer, for non-multithreaded operation.
    For multithreaded ndbd, each thread will set a local jam buffer later.
  */
#ifndef NO_EMULATED_JAM
  void * jamBuffer = (void *)&theEmulatedJamBuffer;
#else
  void * jamBuffer = 0;
#endif
  NdbThread_SetTlsKey(NDB_THREAD_TLS_JAM, jamBuffer);

  NdbMem_Create();

  theConfiguration = new Configuration();
  theWatchDog      = new WatchDog();
  theThreadConfig  = new ThreadConfig();
  theSimBlockList  = new SimBlockList();
  m_socket_server  = new SocketServer();
  m_mem_manager    = new Ndbd_mem_manager();
  globalData.m_global_page_pool.setMutex();

  if (theConfiguration == NULL ||
      theWatchDog == NULL ||
      theThreadConfig == NULL ||
      theSimBlockList == NULL ||
      m_socket_server == NULL ||
      m_mem_manager == NULL )
  {
    ERROR_SET(fatal, NDBD_EXIT_MEMALLOC,
              "Failed to create EmulatorData", "");
  }

  if (!(theShutdownMutex = NdbMutex_Create()))
  {
    ERROR_SET(fatal, NDBD_EXIT_MEMALLOC,
              "Failed to create shutdown mutex", "");
  }
}

void
EmulatorData::destroy(){
  if(theConfiguration)
    delete theConfiguration; theConfiguration = 0;
  if(theWatchDog)
    delete theWatchDog; theWatchDog = 0;
  if(theThreadConfig)
    delete theThreadConfig; theThreadConfig = 0;
  if(theSimBlockList)
    delete theSimBlockList; theSimBlockList = 0;
  if(m_socket_server)
    delete m_socket_server; m_socket_server = 0;
  NdbMutex_Destroy(theShutdownMutex);
  if (m_mem_manager)
    delete m_mem_manager; m_mem_manager = 0;
  
  NdbMem_Destroy();
}
