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


#include <stdarg.h>
#include <stdio.h>      /* vsnprintf */
#include <string.h>


#include "Enclave.h"
#include "Enclave_t.h"  /* print_string */

#include "wa.h"

/* 
 * printf: 
 *   Invokes OCALL to display the enclave buffer to the terminal.
 */
void printf(const char *fmt, ...)
{
    char buf[BUFSIZ] = {'\0'};
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, BUFSIZ, fmt, ap);
    va_end(ap);
    ocall_print_string(buf);
}

/*int ecall_invoke(bool *b, Module *m, char *entry, int argc, char **argv){
    *b = invoke(m, entry, argc, argv);
    return SGX_SUCCESS;
}

int ecall_load_module(uint32_t len, uint8_t* file_contents, Module *m, char *path, Options opts){
    m = load_module(len, file_contents, path, opts);
    printf("nn %d\n", m->function_count);
    return SGX_SUCCESS;
}*/

char* value_repr(char *value, StackValue *v) {
    switch (v->value_type) {
    case I32: snprintf(value, 255, "0x%x:i32",  v->value.uint32); break;
    case I64: snprintf(value, 255, "0x%llx:i64", v->value.uint64); break;
    case F32: snprintf(value, 255, "%.7g:f32",  v->value.f32);    break;
    case F64: snprintf(value, 255, "%.7g:f64",  v->value.f64);    break;
    }
    return value;
}

int ecall_load_invoke_allInOne(uint32_t len, uint8_t* file_contents, char *path, Options opts, bool *b, char *entry, int argc, char **argv, char *output){
    //printf("in ecall\n");
    char value_str[256];
    Module *m = load_module(len, file_contents, path, opts);
    *b = invoke(m, entry, argc, argv);
    memset(output, '\0', 1);
    if (m->sp >= 0) {
        char *temp = value_repr(value_str, &m->stack[m->sp]);
	    snprintf(output, strlen(temp+3), "%s\n", temp);
        //printf("enclave will return string %s\n", output);
    }
    //printf("almost there\n");
    return SGX_SUCCESS;
}
