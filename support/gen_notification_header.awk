BEGIN {
    printf("/* This is a generated file. Don't edit. Edit the source instead.\n */\n\n");

    printf("#ifndef _notifications_h_\n")
    printf("#define _notifications_h_\n")
    printf("\n#include <stdbool.h>\n");
    printf("\n#include <stdint.h>\n");
    printf("#include \"notification_center.h\"\n");
}

/^notification / {
    name=$2
    delete args
    delete argnames
    delete argdef
    delete argtypes
    n=0;
    for(i = 3; i <= NF; i++)
    {
        args[n]=$i
        n++;
    }
    namelist[name]=n
    for(i = 0; i < n; i++)
    {
        match(args[i], /^.*:/)
        argname=substr(args[i], RSTART + RLENGTH, 1024)
        argtype=substr(args[i], RSTART, RLENGTH - 1)

        if(argtype == "int")
        {
            argdef[i] = "int "
        }
        else if(argtype == "uint")
        {
            argdef[i] = "unsigned int "
        }
        else if(argtype == "uint64")
        {
            argdef[i] = "uint64_t "
        }
        else if(argtype == "string")
        {
            argdef[i] = "const char *"
        }
        else if(argtype == "bool")
        {
            argdef[i] = "bool "
        }
        else if(argtype == "double")
        {
            argdef[i] = "double "
        }
        else if(argtype == "pointer")
        {
            argdef[i] = "void *"
        }
        argnames[i] = argname
        argtypes[i] = argtype
    }
    printf("\n\n/*\n * Notification type %s\n */\n\n", name);
    printf("typedef struct nc_%s nc_%s_t;\n", name, name)
    printf("struct nc_%s\n{\n", name);
    for(i = 0; i < n; i++)
    {
        printf("    %s%s;\n", argdef[i], argnames[i])
    }
    printf("};\n\n");
    printf("void nc_send_%s_notification(nc_t *nc", name);
    for(i = 0; i < n; i++)
    {
        printf(", %s%s", argdef[i], argnames[i])
    }
    printf(");\n");

    printf("\ntypedef void (*nc_%s_callback_t)(nc_t *nc, const char *channel,\n" \
           "    nc_%s_t *%s_data, void *user_data);\n", name, name, name);
    
    printf("void nc_add_%s_observer(nc_t *nc,\n" \
           "    nc_%s_callback_t callback, void *user_data);\n", name, name);
}

END {
    printf("\n#endif\n\n")
}

