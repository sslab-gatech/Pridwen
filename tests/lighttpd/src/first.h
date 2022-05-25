#ifndef LI_FIRST_H
#define LI_FIRST_H

#ifdef HAVE_CONFIG_H
# include "config.h"
#else
# ifndef _GNU_SOURCE
#  define _GNU_SOURCE
# endif
#endif

#ifdef HAVE_SGX_LOG_H
#include <stdio.h>
#define sgx_log(...) printf(__VA_ARGS__)
//#define sgx_log(...) fprintf(stderr, __VA_ARGS__)
#else
#define sgx_log(...)
#endif

#ifndef __STDC_WANT_LIB_EXT1__
#define __STDC_WANT_LIB_EXT1__ 1
#endif

#endif
