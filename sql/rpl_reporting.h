#ifndef RPL_REPORTING_H
#define RPL_REPORTING_H

/**
   Maximum size of an error message from a slave thread.
 */
#define MAX_SLAVE_ERRMSG      1024

/**
   Mix-in to handle the message logging and reporting for relay log
   info and master log info structures.

   By inheriting from this class, the class is imbued with
   capabilities to do slave reporting.
 */
class Slave_reporting_capability
{
public:
  /** lock used to synchronize m_last_error on 'SHOW SLAVE STATUS' **/
  mutable pthread_mutex_t err_lock;
  /**
     Constructor.

     @param thread_name Printable name of the slave thread that is reporting.
   */
  Slave_reporting_capability(char const *thread_name)
    : m_thread_name(thread_name)
  {
      pthread_mutex_init(&err_lock, MY_MUTEX_INIT_FAST);
  }

  /**
     Writes a message and, if it's an error message, to Last_Error
     (which will be displayed by SHOW SLAVE STATUS).

     @param level       The severity level
     @param err_code    The error code
     @param msg         The message (usually related to the error
                        code, but can contain more information), in
                        printf() format.
  */
  void report(loglevel level, int err_code, const char *msg, ...) const
    ATTRIBUTE_FORMAT(printf, 4, 5);

  /**
     Clear errors. They will not show up under <code>SHOW SLAVE
     STATUS</code>.
   */
  void clear_error() {
    pthread_mutex_lock(&err_lock);
    m_last_error.clear();
    pthread_mutex_unlock(&err_lock);
  }

  /**
     Error information structure.
   */
  class Error {
    friend class Slave_reporting_capability;
  public:
    Error()
    {
      clear();
    }

    void clear()
    {
      number= 0;
      message[0]= '\0';
    }

    /** Error code */
    uint32 number;
    /** Error message */
    char message[MAX_SLAVE_ERRMSG];
  };

  Error const& last_error() const { return m_last_error; }

  virtual ~Slave_reporting_capability()= 0;
private:
  /**
     Last error produced by the I/O or SQL thread respectively.
   */
  mutable Error m_last_error;

  char const *const m_thread_name;

  // not implemented
  Slave_reporting_capability(const Slave_reporting_capability& rhs);
  Slave_reporting_capability& operator=(const Slave_reporting_capability& rhs);
};

#endif // RPL_REPORTING_H

