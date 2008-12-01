/*
 * Start of xa.h header
 *
 * Define a symbol to prevent multiple inclusions of this header file
 */
#ifndef	XA_H
#define	XA_H

/*
 * Transaction branch identification: XID and NULLXID:
 */
#ifndef XIDDATASIZE

#define	XIDDATASIZE	128		/* size in bytes */
#define	MAXGTRIDSIZE	 64		/* maximum size in bytes of gtrid */
#define	MAXBQUALSIZE	 64		/* maximum size in bytes of bqual */

struct xid_t {
	long formatID;			/* format identifier; -1
					means that the XID is null */
	long gtrid_length;		/* value from 1 through 64 */
	long bqual_length;		/* value from 1 through 64 */
	char data[XIDDATASIZE];
};
typedef	struct xid_t XID;
#endif
#define	XA_OK		0		/* normal execution */
#define	XAER_ASYNC	-2		/* asynchronous operation already
					outstanding */
#define	XAER_RMERR	-3		/* a resource manager error occurred in
					 the transaction branch */
#define	XAER_NOTA	-4		/* the XID is not valid */
#define	XAER_INVAL	-5		/* invalid arguments were given */
#define	XAER_PROTO	-6		/* routine invoked in an improper
					context */
#define	XAER_RMFAIL	-7		/* resource manager unavailable */
#define	XAER_DUPID	-8		/* the XID already exists */
#define	XAER_OUTSIDE	-9		/* resource manager doing work outside
					transaction */
#endif /* ifndef XA_H */
/*
 * End of xa.h header
 */
