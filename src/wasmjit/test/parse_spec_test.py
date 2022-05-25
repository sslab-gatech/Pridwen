#!/usr/bin/python

import sys

def is_hex(s):
    try:
        int(s, 16)
        return True
    except ValueError:
        return False

def parse_file(file_name):
    f = open(file_name, 'r')

    current_fun = ""
    index = 0
    while 1:
        line = f.readline()
        if not line:
            break

        l = line.strip()
        s = l.split('(')
        if len(s) < 3:
            continue

        if s[1].strip() != "assert_return":
            continue

        ss = s[2].split(" ")
        if ss[0].strip() != "invoke":
            continue

        if current_fun != ss[1]:
            current_fun = ss[1]
            if index > 0:
                print "  }"
            print "  {"
            print "    printf(\"" + current_fun.strip('"') + "\\n\");"
            print "    int count = 0;"
            print "    func = module->funcs.elts[" + str(index) + "];"
            find_next = 0
            index += 1

        lc = s[3].split(" ")
        if lc[0].strip() == "i32.const":
            lt = "i32"
            lv = lc[1].strip(')')
            #print "lc - i32 " + lv,
        elif lc[0].strip() == "i64.const":
            lt = "i64"
            lv = lc[1].strip(')')
            #print "lc - i64 " + lv,
        elif lc[0].strip() == "f32.const":
            lt = "f32"
            lv = lc[1].strip(')')
            #print "lc - f32 " + lv,
        elif lc[0].strip() == "f64.const":
            lt = "f64"
            lv = lc[1].strip(')')
            #print "lc - f64 " + lv,

        rc = s[4].split(" ")
        if rc[0].strip() == "i32.const":
            rt = "i32"
            rv = rc[1].strip(')')
            #print "rc - i32",
        elif rc[0].strip() == "i64.const":
            rt = "i64"
            rv = rc[1].strip(')')
            #print "rc - i64",
        elif rc[0].strip() == "f32.const":
            rt = "f32"
            rv = rc[1].strip(')')
            #print "rc - f32",
        elif rc[0].strip() == "f64.const":
            rt = "f64"
            rv = rc[1].strip(')')
            #print "rc - f64",

        fun_def = ""
        if find_next == 0:
            if rt == "i32":
                fun_def += "int((*fun)("
            elif rt == "i64":
                fun_def += "long((*fun)("
            elif rt == "f32":
                fun_def += "float((*fun)("
            elif rt == "f64":
                fun_def += "double((*fun)("

            if lt == "i32":
                fun_def += "int)) = func->compiled_code;"
            elif lt == "i64":
                fun_def += "long)) = func->compiled_code;"
            elif lt == "f32":
                fun_def += "float)) = func->compiled_code;"
            elif lt == "f64":
                fun_def += "double)) = func->compiled_code;"

            print "    " + fun_def
            find_next = 1

        print "    test_" + rt + "(fun(" + lv + "), " + rv + ", &count);"
    print "  }"



def main():
    file_name = sys.argv[1]
    parse_file(file_name)


if __name__ == "__main__":
    main()

