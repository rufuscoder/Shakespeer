BEGIN {
    prefix="tmp"
    struct="tmp_t"
    asserts=1

    printf("/* This is a generated file. Don't edit. Edit the source instead.\n */\n\n");

    printf("#include <stdlib.h>\n");
    printf("#include \"cmd_table.h\"\n");
}

/^ci / { printf("#include %s\n", $2); next; }
/^cp / { prefix=$2; next; }
/^cs / { struct=$2; next; }
/^a / { asserts=$2; next; }
/^c / {
    cmd=$2
    ccmd=cmd
    gsub(/-/, "_", ccmd)
    gsub(/\$/, "", ccmd)
    gsub(/:/, "", ccmd)
    delete args
    delete argnames
    n=0
    ncmd=0
    for(i = 3; i <= NF; i++)
    {
        if(match($i, /^regexp:/))
        {
            match($0, /^.*regexp:/)
            regexp=substr($0, RSTART + RLENGTH, 1024)
            ncmd = n
            break
        }
        else if($i == "*")
        {
            ncmd = -n
            break
        }
        else if($i != "...")
        {
            args[n]=$i
            n++
            ncmd = n
        }
    }
    cmdlist[cmd]=ncmd
    printf("\nstatic int %s_cmd_%s(void *user_data, int argc, char **argv)\n",
           prefix, ccmd)
    printf("{\n")
    printf("    %s *%s = user_data;\n", struct, prefix)
    printf("    if(%s == NULL) return 0;\n", prefix)
    printf("    if(argc != %d) return 0;\n", n)
    for(i = 0; i < n; i++)
    {
        if(index(args[i], ":") > 0)
        {
            split(args[i], a, /:/)

            if(a[1] == "int")
                printf("    int %s = strtoll(argv[%d], NULL, 0);\n", a[2], i)
            else if(a[1] == "uint")
                printf("    unsigned int %s = strtoull(argv[%d], NULL, 0);\n", a[2], i)
            else if(a[1] == "uint64")
                printf("    uint64_t %s = strtoull(argv[%d], NULL, 0);\n", a[2], i)
            else if(a[1] == "string" || a[1] == "path")
                printf("    char *%s = argv[%d];\n", a[2], i)
            else if(a[1] == "bool")
                printf("    int %s = strtoll(argv[%d], NULL, 0);\n", a[2], i)
            else if(a[1] == "double")
                printf("    double %s = strtod(argv[%d], NULL);\n", a[2], i)
            argnames[i] = a[2]
        }
    }
    printf("    if(%s->cb_%s)\n", prefix, ccmd)
    printf("        return %s->cb_%s(%s", prefix, ccmd, prefix)
    for(i = 0; i < n; i++)
    {
        printf(", %s", argnames[i])
    }
    printf(");\n    return 0;\n")
    printf("}\n")
}

END {
    printf("static cmd_t %s_cmds[] = {\n", prefix)
    for(cmd in cmdlist)
    {
        ccmd=cmd
        gsub(/-/, "_", ccmd)
        gsub(/\$/, "", ccmd)
        gsub(/:/, "", ccmd)
        printf("    {\"%s\", %s_cmd_%s, %d},\n",
               cmd, prefix, ccmd, cmdlist[cmd])
    }
    printf("    {0, 0, -1}\n", prefix)
    printf("};\n")

    printf("\n%s *%s_init(void)\n", struct, prefix)
    printf("{\n")
    printf("    %s *%s = calloc(1, sizeof(%s));\n", struct, prefix, struct)
    printf("    return %s;\n}\n\n", prefix)

    printf("int %s_dispatch_command(const char *line, const char *delimiters,\n", prefix)
    printf("        int allow_null_elements, %s *%s)\n", struct, prefix)
    printf("{\n")
    printf("    return cmd_dispatch(line, delimiters, allow_null_elements,\n")
    printf("        %s_cmds, %s);\n", prefix, prefix)
    printf("}\n")
}

