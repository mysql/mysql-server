/* 
**  Virtual I/O library
**  Written by Andrei Errapart <andreie@no.spam.ee>
*/

/*
 * Abstract acceptor.
 */

#ifdef __GNUC__
#pragma interface			/* gcc class implementation */
#endif

VIO_NS_BEGIN

class VioAcceptorFd
{
public:
	virtual		~VioAcceptorFd();
	virtual Vio*	accept(	int	fd) = 0;
};

VIO_NS_END
