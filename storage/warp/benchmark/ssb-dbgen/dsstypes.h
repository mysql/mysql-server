 /* 
 * Sccsid:     @(#)dsstypes.h	2.1.8.1
 *
 * general definitions and control information for the DSS data types
 * and function prototypes
 * Modified for SSBM prototype
 */

/*
 * typedefs
 */
#ifdef SSBM
typedef struct
{
    long            custkey;
    char            name[C_NAME_LEN + 1];
    int             nlen;
    char            address[C_ADDR_MAX + 1];
    int             alen;
    char            city[CITY_FIX+1];
    int             nation_key;
    char            nation_name[C_NATION_NAME_LEN+1];
    int             region_key;
    char            region_name[C_REGION_NAME_LEN+1];
    char            phone[PHONE_LEN + 1];
    char            mktsegment[MAXAGG_LEN + 1];
}               customer_t;
#else
typedef struct
{
    long            custkey;
    char            name[C_NAME_LEN + 1];
    char            address[C_ADDR_MAX + 1];
    int             alen;
    long            nation_code;
    char            phone[PHONE_LEN + 1];
    long            acctbal;
    char            mktsegment[MAXAGG_LEN + 1];
    char            comment[C_CMNT_MAX + 1];
    int             clen;
}               customer_t;
#endif

/* customers.c */
long mk_cust   PROTO((long n_cust, customer_t * c));
int pr_cust    PROTO((customer_t * c, int mode));
int ld_cust    PROTO((customer_t * c, int mode));

#ifdef SSBM

typedef struct
{
    DSS_HUGE	    *okey;  /*for clustering line items*/
    int             linenumber; /*integer, constrain to max of 7*/
    long            custkey;
    long            partkey;
    long            suppkey;
    char            orderdate[DATE_LEN];
    char            opriority[MAXAGG_LEN + 1];
    long            ship_priority;
    long             quantity;
    long           extended_price;
    long           order_totalprice;
    long           discount;
    long           revenue;
    long           supp_cost;
    long           tax;
    char            commit_date[DATE_LEN] ;
    char            shipmode[O_SHIP_MODE_LEN + 1];
}  lineorder_t;
#else
typedef struct
{
    DSS_HUGE	    *okey; 
    long            partkey;
    long            suppkey;
    long            lcnt;
    long            quantity;
    long            eprice;
    long            discount;
    long            tax;
    char            rflag[1];
    char            lstatus[1];
    char            cdate[DATE_LEN];
    char            sdate[DATE_LEN];
    char            rdate[DATE_LEN];
    char           shipinstruct[MAXAGG_LEN + 1];
    char           shipmode[MAXAGG_LEN + 1];
    char           comment[L_CMNT_MAX + 1];
    int            clen;
}               line_t;
#endif

#ifdef SSBM
typedef struct
{
    DSS_HUGE	    *okey;
    long            custkey;
    int             totalprice;
    char            odate[DATE_LEN];
    char            opriority[MAXAGG_LEN + 1];
    char            clerk[O_CLRK_LEN + 1];
    int             spriority;
    long            lines;
    lineorder_t     lineorders[O_LCNT_MAX];
}   order_t;
#else
typedef struct
{
    DSS_HUGE	    *okey;
    long            custkey;
    char            orderstatus;
    long            totalprice;
    char            odate[DATE_LEN];
    char            opriority[MAXAGG_LEN + 1];
    char            clerk[O_CLRK_LEN + 1];
    long            spriority;
    long            lines;
    char            comment[O_CMNT_MAX + 1];
    int            clen;
    line_t          l[O_LCNT_MAX];
}               order_t;
#endif

/* order.c */
long	mk_order	PROTO((long index, order_t * o, long upd_num));
int		pr_order	PROTO((order_t * o, int mode));
int		ld_order	PROTO((order_t * o, int mode));
void	ez_sparse	PROTO((long index, DSS_HUGE *ok, long seq));
#ifndef SUPPORT_64BITS
void	hd_sparse	PROTO((long index, DSS_HUGE *ok, long seq));
#endif

#ifdef SSBM
/*SSBM removes the part supplier table*/       
#else
typedef struct
{
    long            partkey;
    long            suppkey;
    long            qty;
    long            scost;
    char           comment[PS_CMNT_MAX + 1];
    int            clen;
}               partsupp_t;
#endif

#ifdef SSBM
typedef struct
{
    long           partkey;
    char           name[P_NAME_LEN + 1];
    int            nlen;
    char           mfgr[P_MFG_LEN + 1];
    char           category[P_CAT_LEN + 1];
    char           brand[P_BRND_LEN + 1];
    char           color[P_COLOR_MAX + 1];
    int            clen;
    char           type[P_TYPE_MAX + 1];
    int            tlen;
    long            size;
    char           container[P_CNTR_LEN + 1];
}               part_t;
#else
typedef struct
{
    long           partkey;
    char           name[P_NAME_LEN + 1];
    int            nlen;
    char           mfgr[P_MFG_LEN + 1];
    char           brand[P_BRND_LEN + 1];
    char           type[P_TYPE_LEN + 1];
    int            tlen;
    long           size;
    char           container[P_CNTR_LEN + 1];
    long           retailprice;
    char           comment[P_CMNT_MAX + 1];
    int            clen;
    partsupp_t     s[SUPP_PER_PART];
}               part_t;
#endif

/* parts.c */
long mk_part   PROTO((long index, part_t * p));
int pr_part    PROTO((part_t * part, int mode));
int ld_part    PROTO((part_t * part, int mode));

#ifdef SSBM
typedef struct
{
    long            suppkey;
    char            name[S_NAME_LEN + 1];
    char            address[S_ADDR_MAX + 1];
    int             alen; 
    char            city[CITY_FIX +1];
    int             nation_key;
    char            nation_name[S_NATION_NAME_LEN+1];
    int             region_key;
    char            region_name[S_REGION_NAME_LEN+1];
    char            phone[PHONE_LEN + 1];
}               supplier_t;
#else
typedef struct
{
    long            suppkey;
    char            name[S_NAME_LEN + 1];
    char            address[S_ADDR_MAX + 1];
    int             alen;
    long            nation_code;
    char            phone[PHONE_LEN + 1];
    long            acctbal;
    char            comment[S_CMNT_MAX + 1];
    int             clen;
}               supplier_t;
#endif

/* supplier.c */
long mk_supp   PROTO((long index, supplier_t * s));
int pr_supp    PROTO((supplier_t * supp, int mode));
int ld_supp    PROTO((supplier_t * supp, int mode));

#ifdef SSBM
/*todo: add new date table*/

typedef struct
{
   long            datekey;
   char            date[D_DATE_LEN+1];
   char            dayofweek[D_DAYWEEK_LEN+1] ;
   char            month[D_MONTH_LEN+1];
   int             year;
   int             yearmonthnum;
   char            yearmonth[D_YEARMONTH_LEN+1];
   int             daynuminweek;
   int             daynuminmonth;
   int             daynuminyear;
   int             monthnuminyear;
   int             weeknuminyear;
   char            sellingseason[D_SEASON_LEN + 1];
   int             slen;
   char            lastdayinweekfl[2];
   char            lastdayinmonthfl[2];
   char            holidayfl[2];
   char            weekdayfl[2];
}      date_t;

/* date.c */

long mk_date   PROTO((long index, date_t * d));
int pr_date    PROTO((date_t * date, int mode));
int ld_date    PROTO((date_t * date, int mode));

#endif

typedef struct
{
    long            timekey;
    char            alpha[DATE_LEN];
    long            year;
    long            month;
    long            week;
    long            day;
} dss_time_t;               

/* time.c */
long mk_time   PROTO((long index, dss_time_t * t));



/*
 * this assumes that N_CMNT_LEN >= R_CMNT_LEN 
 */
typedef struct
{
    long            code;
    char            *text;
    long            join;
    char            comment[N_CMNT_MAX + 1];
    int             clen;
}               code_t;

/* code table */
int mk_nation   PROTO((long i, code_t * c));
int pr_nation    PROTO((code_t * c, int mode));
int ld_nation    PROTO((code_t * c, int mode));
int mk_region   PROTO((long i, code_t * c));
int pr_region    PROTO((code_t * c, int mode));
int ld_region    PROTO((code_t * c, int mode));
























