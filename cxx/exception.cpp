#include <db.h>
#include <db_cxx.h>

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
	the_what = strdup(db_strerror(the_err));
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
