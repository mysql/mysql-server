#include <db.h>
#include <db_cxx.h>

static char*cpp_strdup (const char *s)
{
    int l=strlen(s)+1;
    char *r = new char[l];
    strncpy(r, s, l);
    return r;
}

DbException::~DbException() throw()
{
    if (the_what!=0) {
	delete [] the_what;
    }
}

DbException::DbException(int err)
    :   the_err(err),
	the_env(0)
{
    FillTheWhat();
}

void DbException::FillTheWhat(void)
{
    if (the_err!=0) {
	the_what = cpp_strdup(db_strerror(the_err));
    }
}

int DbException::get_errno() const
{
    return the_err;
}

const char *DbException::what() const throw()
{
    return the_what;
}

DbEnv *DbException::get_env() const
{
    return the_env;
}

void DbException::set_env(DbEnv *new_env)
{
    the_env = new_env;
}

// Must define a copy constructor so that the delete[] of the same the_what doesn't happen
DbException::DbException (const DbException &that)
    :  std::exception(),
       the_what(cpp_strdup(that.the_what)),
       the_err(that.the_err),
       the_env(that.the_env)
{
}
    

DbException &DbException::operator = (const DbException &that)
{
	if (this != &that) {
	    delete [] the_what;
	    the_what = cpp_strdup(that.the_what);
	    the_err  = that.the_err;
	    the_env  = that.the_env;
	}
	return (*this);
}

DbDeadlockException::DbDeadlockException (DbEnv *env)
    :  DbException(DB_LOCK_DEADLOCK)
{
    this->set_env(env);
}
