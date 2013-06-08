BEGIN {
    prefix="tmp"
    struct="tmp_t"

    printf("/* This is a generated file. Don't edit. Edit the source instead.\n */\n\n");
}

/^si / { includes=sprintf("%s\n#include %s\n", includes, $2); next; }
/^sp / { prefix=$2;  next; }
/^ss / { struct=$2; next; }
/^c / {
    cmd=$2
    gsub(/-/, "_", cmd)
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
            else if(argtype == "uint")
                params=sprintf("%s, unsigned int %s", params, argname);
            else if(argtype == "uint64")
                params=sprintf("%s, uint64_t %s", params, argname);
            else if(argtype == "string" || argtype == "path")
                params=sprintf("%s, const char *%s", params, argname);
            else if(argtype == "bool")
                params=sprintf("%s, int %s", params, argname);
            else if(argtype == "double")
                params=sprintf("%s, double %s", params, argname);
        }
        else if(args[i] == "...")
            params=sprintf("%s, ...", params);
    }
    cmdlist[cmd]=sprintf("int %s_send_%s(%s *%s%s);", prefix, cmd, struct, prefix, params)
}

END {
    printf("#ifndef _%s_send_h_\n", prefix);
    printf("#define _%s_send_h_\n", prefix);
    printf("%s\n", includes);
    for(cmd in cmdlist)
    {
        printf("%s\n", cmdlist[cmd]);
    }
    printf("#endif\n");
}

