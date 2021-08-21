/**
 * Debug macros, modified from `Learn C the Hard Way (Zed Shaw)`
 **/

#ifndef __dbg_h__
#define __dbg_h__
// usual defence against double inclusions

// includes for functions used in macros
#include <stdio.h>
#include <errno.h>
#include <string.h>

// Allow disabling of macros if compiled with NDEBUG
#ifdef NDEBUG
// clears debug macro if needed
#define debug(M, ...)
#else
// macro for parsing error message with printf
#define debug(M, ...) fprintf(stderr,\
        "[DEBUG] (%s:%d) [%s] : " M "\n",\
        __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__)
#endif

// coverts global errno to string
#define clean_errno() (errno == 0 ? "NONE" : strerror(errno))

// logging messages for end user
#define log_err(M, ...) fprintf(stderr,\
        "[ERROR] (%s:%d: errno: %s) [%s] : " M "\n",\
        __FILE__, __LINE__, clean_errno(), __FUNCTION__, ##__VA_ARGS__)

#define log_warn(M, ...) fprintf(stderr,\
        "[WARN]  (%s:%d: errno: %s) [%s] : " M "\n",\
        __FILE__, __LINE__, clean_errno(), __FUNCTION__, ##__VA_ARGS__)

#define log_info(M, ...) fprintf(stderr,\
        "[INFO]  (%s:%d) [%s] : " M "\n",\
        __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__)

// "Best macro": checks if condition A is true, otherwise log error message M
#define check(A, M, ...) if(!(A)) {\
    log_err(M, ##__VA_ARGS__); errno=0; goto error; }

// lower-priority checking, can be disabled if NDEBUG
#define check_debug(A, M, ...) if(!(A)) {\
    debug(M, ##__VA_ARGS__); }

// "2nd best macro": used in func parts that shouldn't run, logs error and jump
#define sentinel(M, ...) {\
    log_err(M, ##__VA_ARGS__); errno=0; goto error; }

// Checks if pointers are valid
#define check_mem(A) check((A), "Out of memory.")

#endif
