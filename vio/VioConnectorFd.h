/*
 * Abstract connector. The file (or socket) descriptor has to be
 * prepared.
 */

#ifdef __GNUC__
#pragma interface			/* gcc class implementation */
#endif

VIO_NS_BEGIN

class VioConnectorFd
{
public:
  virtual ~VioConnectorFd();
  virtual Vio*	connect(int fd) = 0;
};

VIO_NS_END
