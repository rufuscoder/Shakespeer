BEGIN {
    prefix="tmp"
    struct="tmp_t"

    printf("/* This is a generated file. Don't edit. Edit the source instead.\n */\n\n");

    printf("#include <stdlib.h>\n");
    printf("#include <stdio.h>\n");
    printf("#include <inttypes.h>\n");
    printf("#include \"io.h\"\n");
    printf("#include \"log.h\"\n");
}

/^si / { printf("#include %s\n", $2); next; }
/^sp / { prefix=$2; next; }
/^ss / { struct=$2; next; }
/^c / {
    cmd=$2
    ccmd=cmd
    gsub(/-/, "_", ccmd)
    delete args
    delete argnames
    delete argfmt
    delete argdef
    delete argtypes
    n=0;
    for(i = 3; i <= NF; i++)
    {
        args[n]=$i
        n++;
    }
    cmdlist[cmd]=n
    for(i = 0; i < n; i++)
    {
        match(args[i], /^.*:/)
        argname=substr(args[i], RSTART + RLENGTH, 1024)
        argtype=substr(args[i], RSTART, RLENGTH - 1)

        if(argtype == "int")
        {
            argdef[i] = "int "
            argfmt[i] = "%d"
        }
        else if(argtype == "uint")
        {
            argdef[i] = "unsigned int "
            argfmt[i] = "%u"
        }
        else if(argtype == "uint64")
        {
            argdef[i] = "uint64_t "
            argfmt[i] = "%\"PRIu64\""
        }
        else if(argtype == "string" || argtype == "path")
        {
            argdef[i] = "const char *"
            argfmt[i] = "%s"
        }
        else if(argtype == "bool")
        {
            argdef[i] = "int "
            argfmt[i] = "%u"
        }
        else if(argtype == "double")
        {
            argdef[i] = "double "
            argfmt[i] = "%lf"
        }
        argnames[i] = argname
        argtypes[i] = argtype
    }
    printf("\nint %s_send_%s(%s *%s",
           prefix, ccmd, struct, prefix);
    for(i = 0; i < n; i++)
    {
        printf(", %s%s", argdef[i], argnames[i])
    }
    printf(")\n{\n");
    for(i = 0; i < n; i++)
    {
        if(argtypes[i] == "path")
        {
            printf("    char *x_%s = tilde_expand_path(%s);\n", argnames[i], argnames[i]);
            argnames[i] = sprintf("x_%s", argnames[i]);
        }
        else if(argnames[i] == "...")
        {
            printf("    return_val_if_fail(%s, 0);\n", argnames[i-1]);
            printf("    va_list ap;\n");
            printf("    va_start(ap, %s);\n", argnames[i-1]);
            printf("    char *v_%s = 0;\n", argnames[i-1]);
            printf("    int num_returned_bytes = vasprintf(&v_%s, %s, ap);\n", argnames[i-1], argnames[i-1]);
            printf("    if (num_returned_bytes == -1)\n");
            printf("        DEBUG(\"asprintf did not return anything\");");
            printf("    va_end(ap);\n");
            argnames[i-1] = sprintf("v_%s", argnames[i-1]);
            argtypes[i-1] = "...";
            n--
        }
    }
    printf("    int rc = %s_send_command(%s, \"%s", prefix, prefix, cmd);
    for(i = 0; i < n; i++)
    {
        printf("$%s", argfmt[i])
    }
    printf("|\"");
    for(i = 0; i < n; i++)
    {
        if(argtypes[i] == "string")
            printf(", %s ? %s : \"\"", argnames[i], argnames[i])
        else
            printf(", %s", argnames[i])
    }
    printf(");\n");
    for(i = 0; i < n; i++)
    {
        if(argtypes[i] == "path" || argtypes[i] == "...")
            printf("    free(%s);\n", argnames[i])
    }
    printf("    return rc;\n");
    printf("}\n");
}

END {
}

