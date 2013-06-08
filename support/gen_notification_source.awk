BEGIN {
    printf("/* This is a generated file. Don't edit. Edit the source instead.\n */\n\n");

    printf("#include \"notifications.h\"\n");
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
    printf("void nc_send_%s_notification(nc_t *nc", name);
    for(i = 0; i < n; i++)
    {
        printf(", %s%s", argdef[i], argnames[i])
    }
    printf(")\n{\n");
    printf("    nc_%s_t %s_data = {\n", name, name);
    for(i = 0; i < n; i++)
    {
        printf("        .%s = %s", argnames[i], argnames[i])
        if(i < n - 1) printf(",")
        printf("\n")
    }
    printf("    };\n");
    printf("    nc_send_notification(nc, \"%s\", &%s_data);\n", name, name);
    printf("}\n");

    printf("\nvoid nc_add_%s_observer(nc_t *nc, nc_%s_callback_t callback, void *user_data)\n", name, name)
    printf("{\n")
    printf("    nc_add_observer(nc, \"%s\", (nc_callback_t)callback, user_data);\n", name);
    printf("}\n");
}

END {
}

