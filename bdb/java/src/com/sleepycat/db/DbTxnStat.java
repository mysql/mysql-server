/* DO NOT EDIT: automatically built by dist/s_java. */

package com.sleepycat.db;

public class DbTxnStat
{
    public static class Active {
        public int txnid;
        public int parentid;
        public DbLsn lsn;
    };
    public DbLsn st_last_ckp;
    public long st_time_ckp;
    public int st_last_txnid;
    public int st_maxtxns;
    public int st_naborts;
    public int st_nbegins;
    public int st_ncommits;
    public int st_nactive;
    public int st_nrestores;
    public int st_maxnactive;
    public Active st_txnarray[];
    public int st_region_wait;
    public int st_region_nowait;
    public int st_regsize;
}
// end of DbTxnStat.java
