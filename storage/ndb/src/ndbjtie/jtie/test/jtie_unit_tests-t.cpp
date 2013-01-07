/*
 Copyright (c) 2012, Oracle and/or its affiliates. All rights reserved.

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; version 2 of the License.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include <ndb_global.h>
#include <util/NdbTap.hpp>

// platform-specific functions and settings
#ifdef _WIN32
#define SNPRINTF _snprintf_s
#define FILE_SEPARATOR '\\'
#define SCRIPT_FILE_SUFFIX ".cmd"
#define SCRIPT_COMMAND_SEPARATOR "&&"
#else
#define system system
#define SNPRINTF snprintf
#define FILE_SEPARATOR '/'
#define SCRIPT_FILE_SUFFIX ".sh"
#define SCRIPT_COMMAND_SEPARATOR ";"
#endif

/**
 * Runs a test script located in subdirectory.
 */
void run_test_script(const char * this_dir,
                     const char * test_name)
{
    assert(this_dir);
    assert(test_name);

    // directory, name, path of test script to run
    const int path_max = 1024; // FILENAME_MAX is ISO-C but may be huge
    char script_dir[path_max];
    char script_name[path_max];
    char script_path[path_max];
    SNPRINTF(script_dir, sizeof(script_dir), "%s%c%s",
             this_dir, FILE_SEPARATOR, test_name);
    SNPRINTF(script_name, sizeof(script_name), "test_%s" SCRIPT_FILE_SUFFIX,
             test_name);
    SNPRINTF(script_path, sizeof(script_path), "%s%c%s",
             script_dir, FILE_SEPARATOR, script_name);
    //printf("script_dir='%s'\n", script_dir);
    //printf("script_name='%s'\n", script_name);
    //printf("script_path='%s'\n", script_path);

    // try to locate script; also try from this dir's parent dir as
    // multi-config builds may place binaries in a config subdirectory
    char parent_dir[path_max];
    const char * bin_dir = "."; // subdir for binaries
    FILE * script = fopen(script_path, "r");
    if (!script) {
        printf("\nnot found test script at '%s'\n", script_path);

        // re-root script dir and path
        SNPRINTF(parent_dir, sizeof(parent_dir), "%s", this_dir);
        char * sep = strrchr(parent_dir, FILE_SEPARATOR);
        if (sep != NULL) {
            *sep = '\0';
            bin_dir = sep+1;
        }
        SNPRINTF(script_dir, sizeof(script_dir), "%s%c%s",
                 parent_dir, FILE_SEPARATOR, test_name);
        SNPRINTF(script_path, sizeof(script_path), "%s%c%s",
                 script_dir, FILE_SEPARATOR, script_name);
        //printf("parent_dir='%s'\n", parent_dir);
        //printf("script_dir='%s'\n", script_dir);
        //printf("script_path='%s'\n", script_path);

        // try re-rooted path
        script = fopen(script_path, "r");
        if (!script) {
            printf("also not found test script at '%s'\n", script_path);
            // TAP: skip tests (args: count, non-null format string)
            skip(1, "missing script for subtest '%s'", test_name);
            fflush(stdout);
            fflush(stderr);
            return;
        }
    }
    fclose(script);
    printf("\nfound test script at '%s'\n", script_path);
    //printf("bin_dir='%s'\n", bin_dir);

    // run the test script with exit status (using ISO-C's system() call)
    printf("\nTEST: %s\n", test_name);
    char script_cmd[3 * path_max];
    SNPRINTF(script_cmd, sizeof(script_cmd), "cd %s %s .%c%s %s",
             script_dir, SCRIPT_COMMAND_SEPARATOR,
             FILE_SEPARATOR, script_name, bin_dir);
    //printf("script_cmd='%s'\n", script_cmd);
    printf(">>> running '%s'\n", script_cmd);
    fflush(stdout); // system() requires all open streams to be flushed
    fflush(stderr);
    int status = system(script_cmd);
    fflush(stdout);
    fflush(stderr);
    printf("<<< exit status == %d\n", status);
    if (status) {
        fprintf(stderr,
                "------------------------------------------------------------\n"
                "ERROR: failed subtest %s, exit status=%d\n"
                "------------------------------------------------------------\n",
                test_name, status);
    }

    // TAP: report test result (args: passed, non-null format string)
    ok(status == 0, "jtie subtest: %s", test_name);
    fflush(stdout);
    fflush(stderr);
}

int main(int argc, char **argv)
{
    // extract the path and file name by which this program is being called
    // to locate and run the platform test scripts in the subdirectories;
    // convert any forward slashes as when called from perl (even on win).
    for (char * c = *argv; *c != '\0'; c++)
        if (*c == '/') *c = FILE_SEPARATOR;
    const char * this_dir = *argv;
    const char * this_name = *argv;
    char * sep = strrchr(*argv, FILE_SEPARATOR);
    if (sep == NULL) {
        this_dir = ".";
    } else {
        *sep = '\0';
        this_name = sep+1;
    }
    //printf("this_dir='%s'\n", this_dir);
    //printf("this_name='%s'\n", this_name);
    assert(this_dir);
    assert(this_name);

    // TAP: print number of tests to run
    plan(3);

    // run tests
    run_test_script(this_dir, "myapi");
    run_test_script(this_dir, "myjapi");
    // TAP: configured by MYTAP_CONFIG environment var
    // XXX for initial testing: run all
    //if (skip_big_tests) {
    if (false) {
        printf("\n");
        skip(1, "big subtest unload");
    } else {
        run_test_script(this_dir, "unload");
    }

    // TAP: print summary report and return exit status
    return exit_status();
}
