/*
 * Copyright (C) 2011-2018 Intel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Intel Corporation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */


#include <stdio.h>
#include <string.h>
#include <assert.h>

# include <unistd.h>
# include <pwd.h>
# define MAX_PATH FILENAME_MAX

typedef void (*sighandler_t)(int);

#include "sgx_urts.h"
#include "sgx_uswitchless.h"
#include "App.h"
#include "Enclave_u.h"

#ifndef WASM_SPEC_TEST
#define WASM_SPEC_TEST 0
#endif

/* Global EID shared by multiple threads */
sgx_enclave_id_t global_eid = 0;

typedef struct _sgx_errlist_t {
    sgx_status_t err;
    const char *msg;
    const char *sug; /* Suggestion */
} sgx_errlist_t;

/* Error code returned by sgx_create_enclave */
static sgx_errlist_t sgx_errlist[] = {
    {
        SGX_ERROR_UNEXPECTED,
        "Unexpected error occurred.",
        NULL
    },
    {
        SGX_ERROR_INVALID_PARAMETER,
        "Invalid parameter.",
        NULL
    },
    {
        SGX_ERROR_OUT_OF_MEMORY,
        "Out of memory.",
        NULL
    },
    {
        SGX_ERROR_ENCLAVE_LOST,
        "Power transition occurred.",
        "Please refer to the sample \"PowerTransition\" for details."
    },
    {
        SGX_ERROR_INVALID_ENCLAVE,
        "Invalid enclave image.",
        NULL
    },
    {
        SGX_ERROR_INVALID_ENCLAVE_ID,
        "Invalid enclave identification.",
        NULL
    },
    {
        SGX_ERROR_INVALID_SIGNATURE,
        "Invalid enclave signature.",
        NULL
    },
    {
        SGX_ERROR_OUT_OF_EPC,
        "Out of EPC memory.",
        NULL
    },
    {
        SGX_ERROR_NO_DEVICE,
        "Invalid SGX device.",
        "Please make sure SGX module is enabled in the BIOS, and install SGX driver afterwards."
    },
    {
        SGX_ERROR_MEMORY_MAP_CONFLICT,
        "Memory map conflicted.",
        NULL
    },
    {
        SGX_ERROR_INVALID_METADATA,
        "Invalid enclave metadata.",
        NULL
    },
    {
        SGX_ERROR_DEVICE_BUSY,
        "SGX device was busy.",
        NULL
    },
    {
        SGX_ERROR_INVALID_VERSION,
        "Enclave version was invalid.",
        NULL
    },
    {
        SGX_ERROR_INVALID_ATTRIBUTE,
        "Enclave was not authorized.",
        NULL
    },
    {
        SGX_ERROR_ENCLAVE_FILE_ACCESS,
        "Can't open enclave file.",
        NULL
    },
};

/* Check error conditions for loading enclave */
void print_error_message(sgx_status_t ret)
{
    size_t idx = 0;
    size_t ttl = sizeof sgx_errlist/sizeof sgx_errlist[0];

    for (idx = 0; idx < ttl; idx++) {
        if(ret == sgx_errlist[idx].err) {
            if(NULL != sgx_errlist[idx].sug)
                printf("Info: %s\n", sgx_errlist[idx].sug);
            printf("Error: %s\n", sgx_errlist[idx].msg);
            break;
        }
    }
    
    if (idx == ttl)
    	printf("Error code is 0x%X. Please refer to the \"Intel SGX SDK Developer Reference\" for more details.\n", ret);
}

/* Initialize the enclave:
 *   Call sgx_create_enclave to initialize an enclave instance
 */
int initialize_enclave(const sgx_uswitchless_config_t* us_config)
{
    sgx_status_t ret = SGX_ERROR_UNEXPECTED;

    /* Call sgx_create_enclave to initialize an enclave instance */
    /* Debug Support: set 2nd parameter to 1 */

    const void* enclave_ex_p[32] = { 0 };

    enclave_ex_p[SGX_CREATE_ENCLAVE_EX_SWITCHLESS_BIT_IDX] = (const void*)us_config;

    ret = sgx_create_enclave_ex(ENCLAVE_FILENAME, SGX_DEBUG_FLAG, NULL, NULL, &global_eid, NULL, SGX_CREATE_ENCLAVE_EX_SWITCHLESS, enclave_ex_p);
    if (ret != SGX_SUCCESS) {
        print_error_message(ret);
        return -1;
    }

    return 0;
}

#include "ocall.cpp"

/* Application entry */
int SGX_CDECL main(int argc, char *argv[])
{
    char *filename;
    /* Configuration for Switchless SGX */
    sgx_uswitchless_config_t us_config = SGX_USWITCHLESS_CONFIG_INITIALIZER;
    us_config.num_uworkers = 2;

    /* Initialize the enclave */
    if(initialize_enclave(&us_config) < 0)
    {
        printf("Error: enclave initialization failed\n");
        return -1;
    }

    if (argc < 2) {
        fprintf(stderr, "Need an input .wasm file\n");
        return -1;
    }

    filename = argv[1];
    
#if !WASM_SPEC_TEST
    enclave_main(global_eid, filename);
#else // do spec test.
    if (argc < 3) {
        fprintf(stderr, "Usage: ./app [.wasm] [target_fun] [# args] [value type ...] [expected value expected type]\n");
        return -1;
    }
    char *fun_name = argv[2];
    size_t n_args = atoi(argv[3]);
    uint64_t *args = NULL;
    uint8_t *args_type = NULL;
    uint64_t expected = 0;
    uint8_t expected_type;
    size_t counter = 4;

    if (n_args > 0) {
      args = (uint64_t *) calloc(1, sizeof(uint64_t) * n_args);
      args_type = (uint8_t *) malloc(sizeof(uint8_t) * n_args);
      for (size_t i = 0; i < n_args; i++) {
        args_type[i] = atoi(argv[counter + 1]);
	switch (args_type[i]) {
	  case 0: // i32
	    args[i] = (uint32_t) strtoul(argv[counter], 0, 10);
	    //args[i] = atoi(argv[counter]);
	    break;
	  case 1: // i64
	    args[i] = strtoul(argv[counter], 0, 10);
	    //args[i] = atol(argv[counter]); 
	    break;
	  case 2: { // f32
	    float f = (float) atof(argv[counter]);
	    memcpy(&args[i], &f, sizeof(uint32_t));
	    break;
          }
	  case 3: { // f64
	    double f = atof(argv[counter]);
	    memcpy(&args[i], &f, sizeof(uint64_t));
	    break;
          }
	  case 5: { // f32 with i32 encoding
	    args[i] = (uint32_t) strtoul(argv[counter], 0, 10);
	    args_type[i] = 2;
	    break;
          }
	  case 6: { // f64 wwith i64 encoding
	    args[i] = strtoul(argv[counter], 0, 10);
            args_type[i] = 3;
	    break;
          }
	  default:
	    printf("unknown input type!\n");
	    break;
	}
	counter += 2;
      }
    }
    expected_type = atoi(argv[counter + 1]);
    switch (expected_type) {
      case 0: // i32
	expected = (uint32_t) strtoul(argv[counter], 0, 10);
        //expected = atoi(argv[counter]);
        break;
      case 1: // i64
	expected = strtoul(argv[counter], 0, 10);
        //expected = atol(argv[counter]); 
        break;
      case 2: { // f32
	    float f = (float) atof(argv[counter]);
	    memcpy(&expected, &f, sizeof(uint32_t));
        break;
      }
      case 3: { // f64
	    double f = atof(argv[counter]);
	    memcpy(&expected, &f, sizeof(uint64_t));
        break;
      }
      case 4: // void
        break;
      case 5: { // f32 with i32 encoding
	expected = (uint32_t) strtoul(argv[counter], 0, 10);
        //expected = atoi(argv[counter]);
	expected_type = 2;
        break;
      }
      case 6: { // f64 with i64 encoding
	expected = strtoul(argv[counter], 0, 10);
        //expected = atol(argv[counter]); 
	expected_type = 3;
        break;
      }
      default:
        printf("unknown input type!\n");
        break;
    }

    enclave_spec_test(global_eid, filename, fun_name,
                      args, n_args * sizeof(uint64_t),
                      args_type, n_args,
		      expected, expected_type);
 #endif
    /* Destroy the enclave */
    sgx_destroy_enclave(global_eid);

    return 0;
}

