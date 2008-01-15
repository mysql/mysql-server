//NOTE: fmemopen does not exist in OSX
// I want fmemopen
#define _GNU_SOURCE

#include "test.h"
#include <assert.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdio.h>

const char *expect_errpfx=0;
int n_handle_error=0;

void handle_error (const DB_ENV *dbenv, const char *errpfx, const char *msg) {
    assert(errpfx==expect_errpfx);
    n_handle_error++;
}
int main (int argc, const char *argv[]) {
    parse_args(argc, argv);
    
    system("rm -rf " DIR);
    int r=mkdir(DIR, 0777); assert(r==0);

    {
	DB_ENV *env;
	r = db_env_create(&env, 0); assert(r==0);
	r = env->open(env, DIR, -1, 0644);
	assert(r==EINVAL);
	assert(n_handle_error==0);
	r = env->close(env, 0); assert(r==0);
    }

    int do_errfile, do_errcall;
    for (do_errfile=0; do_errfile<2; do_errfile++) {
	for (do_errcall=0; do_errcall<2; do_errcall++) {
	    DB_ENV *env;
	    char buf[10000]="";
	    FILE *write_here = fmemopen(buf, sizeof(buf), "w");
	    n_handle_error=0;
	    r = db_env_create(&env, 0); assert(r==0);
	    if (do_errfile)
		env->set_errfile(env, write_here);
	    if (do_errcall) 
		env->set_errcall(env, handle_error);
	    r = env->open(env, DIR, -1, 0644);
	    assert(r==EINVAL);
	    r = env->close(env, 0); assert(r==0);
	    fclose(write_here);
	    if (do_errfile) {
		assert(buf[0]!=0); 
		assert(buf[0]!=':');
	    } else {
		assert(buf[0]==0);
	    }
	    if (do_errcall) {
		assert(n_handle_error==1);
	    } else {
		assert(n_handle_error==0);
	    }
	}
    }

    return 0;
}
