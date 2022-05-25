#!/usr/bin/python

import sys
import json
import os
import subprocess
from ctypes import *

def is_hex(s):
    try:
        int(s, 16)
        return True
    except ValueError:
        return False

def to_float(s, t):
    i = int(s)
    if t == 'f32':
        cp = pointer(c_int(i))
        fp = cast(cp, POINTER(c_float))
        return str(fp.contents.value)
    elif t == 'f64':
        if i == 13830554455654793215:
            return '-0x1.fffffffffffffp-1'
        #if i == 4890909195324358655:
        #    return '9223372036854774784.0'
        #if i == 14114281232179134464:
        #    return '-9223372036854775808.0'
        #if i == 4895412794951729151:
        #    return '18446744073709549568.0'
        #if i == 4890909195324358656:
        #    return '9223372036854775808'
        cp = pointer(c_long(i))
        fp = cast(cp, POINTER(c_double))
        return str(fp.contents.value)
    return ''

def to_signed_value(n, t):
    if t == 'i32':
        n = int(n) & 0xffffffff
        return str((n ^ 0x80000000) - 0x80000000)
    elif t == 'i64':
        n = int(n) & 0xffffffffffffffff
        return str((n ^ 0x8000000000000000) - 0x8000000000000000)
    elif t == 'f32':
        return n
        #return to_float(n, t)
    elif t == 'f64':
        return n
        #return to_float(n, t)
    

def run_test(path, data):
    app = './app'
    args = ''
    wasm = ''
    fun = ''

    commands = data['commands']
    for i in range(0, len(commands)):
        fun = ''
        n_args = ''
        args = ''
        ret = ''

        c = commands[i]
        if 'filename' in c:
            wasm = c['filename']
            continue
       
        if 'type' in c:
            if c['type'] != 'assert_return':
                continue

        if "action" not in c:
            continue
        
        action = c['action']
        if 'field' in action:
            fun = action['field']

        if 'args' in action:
            n_args = str(len(action['args']))
            for arg in action['args']:
                #args += arg['value'] + ' '
                args += to_signed_value(arg['value'], arg['type']) + ' '
                if arg['type'] == 'i32':
                    args += '0 '
                elif arg['type'] == 'i64':
                    args += '1 '
                elif arg['type'] == 'f32':
                    #args += '2 '
                    args += '5 '
                elif arg['type'] == 'f64':
                    #args += '3 '
                    args += '6 '
                else:
                    print "unknwon arg type"
                    return

        if 'expected' not in c:
            continue

        if len(c['expected']) == 0:
            ret = '0 4'
        elif len(c['expected']) == 1:
            etype = c['expected'][0]['type']
            #evalue = c['expected'][0]['value']
            evalue = to_signed_value(c['expected'][0]['value'], etype)
            ret = evalue + ' '
            if etype == 'i32':
                ret += '0'
            elif etype == 'i64':
                ret += '1'
            elif etype == 'f32':
                #ret += '2'
                ret += '5'
            elif etype == 'f64':
                #ret += '3'
                ret += '6'
            else:
                print "unknown ret type"
                return

        cmd =  app + ' ' + path + '/' + wasm + ' ' + fun + ' ' + n_args + ' ' + args + ret
        #print cmd
        #os.system(cmd)
        t = cmd.split(' ')
        out = subprocess.Popen(t, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        stdout, stderr = out.communicate()
        if 'pass' not in stdout:
            print 'fail: ' + cmd + ' ' + stdout
        else:
            print 'pass: ' + wasm + ' ' + fun + ' ' + n_args + ' ' + args + ret
 
def main():
    file_name = sys.argv[1]
    json_file = open(file_name)
    data = json.load(json_file)

    path = os.path.dirname(file_name)
    run_test(path, data)

if __name__ == "__main__":
    main()

