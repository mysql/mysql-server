/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#ifndef NDBD_EXIT_CODES_H
#define NDBD_EXIT_CODES_H

#ifdef __cplusplus
extern "C" {
#endif

#ifndef DOXYGEN_SHOULD_SKIP_INTERNAL

/**
 * Exit error codes for NDBD
 *
 * These errorcodes should be used whenever a condition
 * is detected where it's necesssary to shutdown NDB.
 *
 * Example: When another node fails while a NDB node are performing
 * a system restart the node should be shutdown. This 
 * is kind of an error but the cause of the error is known
 * and a proper errormessage describing the problem should
 * be printed in error.log. It's therefore important to use
 * the proper errorcode.
 *
 */

typedef enum
{
  ndbd_exit_st_success = 0,
  ndbd_exit_st_unknown = 1,
  ndbd_exit_st_permanent = 2,
  ndbd_exit_st_temporary = 3,
  ndbd_exit_st_filesystem_error = 4
} ndbd_exit_status_enum;

typedef enum
{
  ndbd_exit_cl_none = 0,
  ndbd_exit_cl_unknown = 1,
  ndbd_exit_cl_internal_error = 2,
  ndbd_exit_cl_configuration_error = 3,
  ndbd_exit_cl_arbitration_error = 4,
  ndbd_exit_cl_restart_error = 5,
  ndbd_exit_cl_resource_configuration_error = 6,
  ndbd_exit_cl_filesystem_full_error = 7,
  ndbd_exit_cl_filesystem_inconsistency_error = 8,
  ndbd_exit_cl_filesystem_limit = 9
} ndbd_exit_classification_enum;

typedef ndbd_exit_status_enum ndbd_exit_status;
typedef ndbd_exit_classification_enum ndbd_exit_classification;

/* Errorcodes before block division was used */
#define NDBD_EXIT_GENERIC                     2300
#define NDBD_EXIT_PRGERR                      2301
#define NDBD_EXIT_NODE_NOT_IN_CONFIG          2302
#define NDBD_EXIT_SYSTEM_ERROR                2303
#define NDBD_EXIT_INDEX_NOTINRANGE            2304
#define NDBD_EXIT_ARBIT_SHUTDOWN              2305
#define NDBD_EXIT_POINTER_NOTINRANGE          2306
#define NDBD_EXIT_PARTITIONED_SHUTDOWN        2307
#define NDBD_EXIT_SR_OTHERNODEFAILED          2308
#define NDBD_EXIT_NODE_NOT_DEAD               2309
#define NDBD_EXIT_SR_REDOLOG                  2310
#define NDBD_EXIT_SR_RESTARTCONFLICT          2311
#define NDBD_EXIT_NO_MORE_UNDOLOG             2312 
#define NDBD_EXIT_SR_UNDOLOG                  2313 
#define NDBD_EXIT_SINGLE_USER_MODE            2314 
#define NDBD_EXIT_NODE_DECLARED_DEAD          2315 
#define NDBD_EXIT_SR_SCHEMAFILE               2316
#define NDBD_EXIT_MEMALLOC                    2327
#define NDBD_EXIT_BLOCK_JBUFCONGESTION        2334
#define NDBD_EXIT_TIME_QUEUE_SHORT            2335
#define NDBD_EXIT_TIME_QUEUE_LONG             2336
#define NDBD_EXIT_TIME_QUEUE_DELAY            2337
#define NDBD_EXIT_TIME_QUEUE_INDEX            2338
#define NDBD_EXIT_BLOCK_BNR_ZERO              2339
#define NDBD_EXIT_WRONG_PRIO_LEVEL            2340
#define NDBD_EXIT_NDBREQUIRE                  2341
#define NDBD_EXIT_ERROR_INSERT                2342
#define NDBD_EXIT_NDBASSERT                   2343
#define NDBD_EXIT_INVALID_CONFIG              2350
#define NDBD_EXIT_OUT_OF_LONG_SIGNAL_MEMORY   2351

/* Errorcodes for fatal resource errors */
#define NDBD_EXIT_RESOURCE_ALLOC_ERROR        2500

#define NDBD_EXIT_OS_SIGNAL_RECEIVED          6000

/* VM 6050-> */
#define NDBD_EXIT_WATCHDOG_TERMINATE          6050
#define NDBD_EXIT_SIGNAL_LOST                 6051
#define NDBD_EXIT_SIGNAL_LOST_SEND_BUFFER_FULL 6052
#define NDBD_EXIT_ILLEGAL_SIGNAL              6053
#define NDBD_EXIT_CONNECTION_SETUP_FAILED     6054

/* NDBCNTR 6100-> */
#define NDBD_EXIT_RESTART_TIMEOUT             6100
#define NDBD_EXIT_RESTART_DURING_SHUTDOWN     6101

/* TC  6200-> */
/* DIH 6300-> */
#define NDBD_EXIT_MAX_CRASHED_REPLICAS        6300
#define NDBD_EXIT_MASTER_FAILURE_DURING_NR    6301
#define NDBD_EXIT_LOST_NODE_GROUP             6302
#define NDBD_EXIT_NO_RESTORABLE_REPLICA       6303

/* ACC 6600-> */
#define NDBD_EXIT_SR_OUT_OF_INDEXMEMORY       6600
/* TUP 6800-> */
#define NDBD_EXIT_SR_OUT_OF_DATAMEMORY        6800
/* LQH 7200-> */


/* Errorcodes for NDB filesystem */
#define NDBD_EXIT_AFS_NOPATH                2801
/*
#define NDBD_EXIT_AFS_CHANNALFULL           2802
#define NDBD_EXIT_AFS_NOMORETHREADS         2803
*/
#define NDBD_EXIT_AFS_PARAMETER             2804
#define NDBD_EXIT_AFS_INVALIDPATH           2805
#define NDBD_EXIT_AFS_MAXOPEN               2806
#define NDBD_EXIT_AFS_ALREADY_OPEN          2807

#define NDBD_EXIT_AFS_ENVIRONMENT           2808
#define NDBD_EXIT_AFS_TEMP_NO_ACCESS        2809
#define NDBD_EXIT_AFS_DISK_FULL             2810
#define NDBD_EXIT_AFS_PERMISSION_DENIED     2811
#define NDBD_EXIT_AFS_INVALID_PARAM         2812
#define NDBD_EXIT_AFS_UNKNOWN               2813
#define NDBD_EXIT_AFS_NO_MORE_RESOURCES     2814
#define NDBD_EXIT_AFS_NO_SUCH_FILE          2815
#define NDBD_EXIT_AFS_READ_UNDERFLOW        2816

#define NDBD_EXIT_INVALID_LCP_FILE          2352
#define NDBD_EXIT_INSUFFICENT_NODES         2353

const char *
ndbd_exit_message(int faultId, ndbd_exit_classification *cl);
const char *
ndbd_exit_classification_message(ndbd_exit_classification classification,
			        ndbd_exit_status *status);
const char *
ndbd_exit_status_message(ndbd_exit_status status);

#endif

#ifdef __cplusplus
}
#endif

#endif /* NDBD_EXIT_CODES_H */
