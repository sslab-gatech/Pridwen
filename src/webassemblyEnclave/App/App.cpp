/*
 * Copyright (C) 2011-2017 Intel Corporation. All rights reserved.
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
#include <getopt.h>

# include <unistd.h>
# include <pwd.h>
# define MAX_PATH FILENAME_MAX

#include "sgx_urts.h"
#include "App.h"
#include "Enclave_u.h"

#ifdef USE_READLINE
  // WARNING: GPL license implications
  #include <readline/readline.h>
  #include <readline/history.h>
#else
  #include <editline/readline.h>
#endif

#include "../Enclave/wa.h"
//#include "../Enclave/util.h"



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
        printf("Error: Unexpected error occurred.\n");
}

/* Initialize the enclave:
 *   Step 1: try to retrieve the launch token saved by last transaction
 *   Step 2: call sgx_create_enclave to initialize an enclave instance
 *   Step 3: save the launch token if it is updated
 */
int initialize_enclave(sgx_enclave_id_t* enclave_id)
{
    char token_path[MAX_PATH] = {'\0'};
    sgx_launch_token_t token = {0};
    sgx_status_t ret = SGX_ERROR_UNEXPECTED;
    int updated = 0;
    
    /* Step 1: try to retrieve the launch token saved by last transaction 
     *         if there is no token, then create a new one.
     */
    /* try to get the token saved in $HOME */
    const char *home_dir = getpwuid(getuid())->pw_dir;
    
    if (home_dir != NULL && 
        (strlen(home_dir)+strlen("/")+sizeof(TOKEN_FILENAME)+1) <= MAX_PATH) {
        /* compose the token path */
        strncpy(token_path, home_dir, strlen(home_dir));
        strncat(token_path, "/", strlen("/"));
        strncat(token_path, TOKEN_FILENAME, sizeof(TOKEN_FILENAME)+1);
    } else {
        /* if token path is too long or $HOME is NULL */
        strncpy(token_path, TOKEN_FILENAME, sizeof(TOKEN_FILENAME));
    }

    FILE *fp = fopen(token_path, "rb");
    if (fp == NULL && (fp = fopen(token_path, "wb")) == NULL) {
        printf("Warning: Failed to create/open the launch token file \"%s\".\n", token_path);
    }

    if (fp != NULL) {
        /* read the token from saved file */
        size_t read_num = fread(token, 1, sizeof(sgx_launch_token_t), fp);
        if (read_num != 0 && read_num != sizeof(sgx_launch_token_t)) {
            /* if token is invalid, clear the buffer */
            memset(&token, 0x0, sizeof(sgx_launch_token_t));
            printf("Warning: Invalid launch token read from \"%s\".\n", token_path);
        }
    }
    /* Step 2: call sgx_create_enclave to initialize an enclave instance */
    /* Debug Support: set 2nd parameter to 1 */
    ret = sgx_create_enclave(ENCLAVE_FILENAME, SGX_DEBUG_FLAG, &token, &updated, enclave_id, NULL);
    if (ret != SGX_SUCCESS) {
        print_error_message(ret);
        if (fp != NULL) fclose(fp);
        return -1;
    }

    /* Step 3: save the launch token if it is updated */
    if (updated == FALSE || fp == NULL) {
        /* if the token is not updated, or file handler is invalid, do not perform saving */
        if (fp != NULL) fclose(fp);
        return 0;
    }

    /* reopen the file with write capablity */
    fp = freopen(token_path, "wb", fp);
    if (fp == NULL) return 0;
    size_t write_num = fwrite(token, 1, sizeof(sgx_launch_token_t), fp);
    if (write_num != sizeof(sgx_launch_token_t))
        printf("Warning: Failed to save launch token to \"%s\".\n", token_path);
    fclose(fp);
    return 0;
}

/* OCall functions */
void ocall_print_string(const char *str)
{
    /* Proxy/Bridge will check the length and null-terminate 
     * the input string to prevent buffer overflow. 
     */
    printf("%s", str);
}

// open and mmap a file
/* Move this functionality out of enclave via ocall
uint8_t *mmap_file(char *path, uint32_t *len) {
    int          fd;
    struct stat  sb;
    uint8_t     *bytes;

    fd = open(path, O_RDONLY);
    if (fd < 0) { FATAL("could not open file '%s'\n", path); }
    if (fstat(fd, &sb) < 0) { FATAL("could stat file '%s'\n", path); }

    bytes = mmap(0, sb.st_size, PROT_READ, MAP_SHARED, fd, 0);
    if (len) {
        *len = sb.st_size;  // Return length if requested
    }
    if (bytes == MAP_FAILED) { FATAL("could not mmap file '%s'", path); }
    return bytes;
}*/

// Split a space separated strings into an array of strings
// Returns 0 on failure
// Memory must be freed by caller
// Based on: http://stackoverflow.com/a/11198630/471795
char **split_string(char *str, int *count) {
    char **res = NULL;
    char  *p   = strtok(str, " ");
    int    idx = 0;

    // split string and append tokens to 'res'
    while (p) {
        res = (char**)realloc(res, sizeof(char*) * idx+1);
        if (res == NULL) {
            return 0;
        }

        res[idx++] = p;
        p = strtok(NULL, " ");
    }

    /* realloc one extra element for the last NULL */

    res = (char**)realloc (res, sizeof(char*) * (idx+1));
    res[idx] = 0;

    if (count) { *count = idx; }
    return res;
}

void usage(char *prog) {
    fprintf(stderr, "%s [--debug] WASM_FILE [--repl|-- ARG...]\n", prog);
    exit(2);
}

// Special test imports
uint32_t _spectest__global_ = 666;

void _spectest__print_(uint32_t val) {
    //printf("spectest.print 0x%x:i32\n", val);
    printf("0x%x:i32\n", val);
}


/* Application entry */
int SGX_CDECL main(int argc, char *argv[])
{
    //(void)(argc);
    //(void)(argv);
    sgx_status_t status = SGX_SUCCESS;
    sgx_enclave_id_t enclave_id = 0;

    /* Initialize the enclave */
    if(initialize_enclave(&enclave_id) < 0){
        printf("Enter a character before exit ...\n");
        getchar();
        return -1; 
    }
    
    char   *mod_path, *entry, *line;
    int     repl = 0, debug = 0, res = 0;

    // Parse arguments
    int option_index = 0, c;
    struct option long_options[] = {
        {"repl",  no_argument, &repl,  1},
        {"debug", no_argument, &debug, 1},
        {0,       0,           0,      0}
    };
    while ((c = getopt_long (argc, argv, "",
                             long_options, &option_index)) != -1) {
        switch (c) {
        case 0: break;
        case '?': usage(argv[0]); break;
        default: usage(argv[0]);
        }
    }
    if (optind >= argc) { usage(argv[0]); }
    mod_path = argv[optind++];

    if (debug) {
        printf("repl: %d, debug: %d, module path: %s\n",
               repl, debug, mod_path);
    }

    // Load the module
    Options opts;
    Module *m;
    //Module *m = load_module(mod_path, opts);
    uint8_t  *bytes;
    uint32_t *len;

    FILE *f = fopen(mod_path, "rb");
    fseek(f, 0, SEEK_END);
    len = (uint32_t*)malloc(sizeof(uint32_t));
    int t = ftell(f);
    memcpy(len, &t, 4);
    //*len = ftell(f);
    fseek(f, 0, SEEK_SET);  //same as rewind(f);
    bytes = (uint8_t*)malloc(*len);
    fread(bytes, *len, 1, f);
    fclose(f);
    //printf("here");fflush(stdout);
    //uint8_t *bytes, char *path, uint32_t *len
    //ecall_load_module(enclave_id, (int*)&status, *len, bytes,  m, mod_path, opts);
    //printf("ladeda\n");fflush(stdout);
    //printf("here in app.cpp %d\n", m->function_count);fflush(stdout);

    //char value_str[256];
    char *output = (char*)malloc(256*sizeof(char));
    memset(output, '\0', 1);
    if (!repl) {
        // Invoke one function and exit
        //res = invoke(m, argv[optind], argc-optind-1, argv+optind+1);
        //printf("here");fflush(stdout);
        //ecall_invoke(enclave_id, (int*)&status, (bool*)&res, m, argv[optind], argc-optind-1, argv+optind+1);
        //printf("here");fflush(stdout);
        ecall_load_invoke_allInOne(enclave_id, (int*)&status, *len, bytes, mod_path, opts, (bool*)&res, argv[optind], argc-optind-1, argv+optind+1, output);

        if (res) {
                    printf("%s\n", output);
	    //if (m->sp >= 0) {
		//printf("%s\n", value_repr(value_str, &m->stack[m->sp]));
	    //}
        } else {
	    //error("Exception: %s\n", exception); removing these since we don't have the global exception available outside the enclave 
        printf("Exception\n");
	    exit(1);
	}
    } else {
        // Simple REPL
        if (optind < argc) { usage(argv[0]); }
        while (line = readline("webassembly> ")) {
            memset(output, '\0', 1);
            int token_cnt = 0;
            char **tokens = split_string(line, &token_cnt);
            if (token_cnt == 0) { continue; }

            // Reset the stacks
            /*m->sp = -1;
            m->fp = -1;
            m->csp = -1;*/
            //res = invoke(m, tokens[0], token_cnt-1, tokens+1);
            //ecall_invoke(enclave_id, (int*)&status, (bool*)&res, m, tokens[0], token_cnt-1, tokens+1);
            ecall_load_invoke_allInOne(enclave_id, (int*)&status, *len, bytes, mod_path, opts, (bool*)&res, argv[optind], argc-optind-1, argv+optind+1, output);

	    if (res) {
            printf("%s\n", output);

		//if (m->sp >= 0) {
		//    printf("%s\n", value_repr(value_str, &m->stack[m->sp]));
		//}
	    } else {
		//error("Exception: %s\n", exception);
        printf("Exception\n");

	    }
            free(tokens);
        }
    }
 
    /* Utilize edger8r attributes */
    //edger8r_array_attributes();
    //edger8r_pointer_attributes();
    //edger8r_type_attributes();
    //edger8r_function_attributes();
    
    /* Utilize trusted libraries */
    //ecall_libc_functions();
    //ecall_libcxx_functions();
    //ecall_thread_functions();
    
    

    /* Destroy the enclave */
    sgx_destroy_enclave(enclave_id);
    
    printf("Info: SampleEnclave successfully returned.\n");

    printf("Enter a character before exit ...\n");
    getchar();
    return 0;
}

