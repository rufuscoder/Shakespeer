BEGIN {
    prefix="tmp"
    struct="tmp_t"

    printf("/* This is a generated file. Don't edit. Edit the source instead.\n */\n\n");
}

/^cp / { prefix=$2;  next; }
/^cs / { struct=$2; next; }
/^chi / { includes=sprintf("%s#include %s\n", includes, $2); next; }
/^m / { members=sprintf("%s    %s;\n", members, substr($0, 3, 1024)); next; }
/^c / {
    cmd=$2
    gsub(/-/, "_", cmd)
    gsub(/\$/, "", cmd)
    gsub(/:/, "", cmd)
    delete args
    params=""
    n=0;
    for(i = 3; i <= NF; i++)
    {
        args[n]=$i
        n++
    }
    for(i = 0; i < n; i++)
    {
        if(match(args[i], /^.*:/))
        {
            argname=substr(args[i], RSTART + RLENGTH, 1024)
            argtype=substr(args[i], RSTART, RLENGTH - 1)

            if(argtype == "int")
                params=sprintf("%s, int %s", params, argname);
            if(argtype == "uint")
                params=sprintf("%s, unsigned int %s", params, argname);
            if(argtype == "uint64")
                params=sprintf("%s, uint64_t %s", params, argname);
            else if(argtype == "string" || argtype == "path")
                params=sprintf("%s, const char *%s", params, argname);
            else if(argtype == "bool")
                params=sprintf("%s, int %s", params, argname);
            else if(argtype == "double")
                params=sprintf("%s, double %s", params, argname);
        }
    }
    cmdlist[cmd]=sprintf("int (*cb_%s)(%s *%s%s);", cmd, struct, prefix, params)
}

END {
    printf("#ifndef _%s_cmd_h_\n", prefix);
    printf("#define _%s_cmd_h_\n", prefix);
    printf("\n#include <inttypes.h>\n");
    printf("%s\n", includes);
    printf("typedef struct %s %s;\n", prefix, struct)
    printf("struct %s\n{\n", prefix)
    printf("%s\n", members);
    for(cmd in cmdlist)
    {
        printf("    %s\n", cmdlist[cmd]);
    }
    printf("};\n\n");
    printf("%s *%s_init(void);\n\n", struct, prefix);
    printf("int %s_dispatch_command(const char *line, const char *delimiters,\n", prefix);
    printf("        int allow_null_elements, %s *%s);\n", struct, prefix);
    printf("#endif\n");
}

