#include "shakespeer.h"

int func_set_hash_prio(sp_t *sp, arg_t *args)
{
    int prio = atoi(args->argv[1]);
    sp_send_set_hash_prio(sp, prio ? prio - 1 : 0);
    return 0;
}

int func_debug(sp_t *sp, arg_t *args)
{
    sp_send_log_level(sp, args->argv[1]);
    return 0;
}

int func_exit(sp_t *sp, arg_t *args)
{
    cmd_fini();
    exit(0);
}

int func_connect(sp_t *sp, arg_t *args)
{
    char *nick = 0;
    char *email = 0;
    char *description = 0;
    char *speed = 0;

    if(args->argc > 2)
        nick = args->argv[2];
    else
    {
        if(cfg_size(cfg, "default-nick"))
            nick = cfg_getstr(cfg, "default-nick");
    }

    if(args->argc > 3)
        email = args->argv[3];
    else
    {
        email = cfg_getstr(cfg, "email");
    }

    if(args->argc > 4)
        description = args->argv[4];
    else
    {
        description = cfg_getstr(cfg, "description");
    }

    if(args->argc > 5)
        speed = args->argv[5];
    else
    {
        speed = cfg_getstr(cfg, "connection");
    }

    if(nick == 0 || *nick == 0)
    {
        printf("need a nick\n");
        return 0;
    }

    sp_send_connect(sp, args->argv[1], nick, email, description, speed, passive_mode, NULL, NULL /* default encoding */);

    return 0;
}

int func_hublist(sp_t *sp, arg_t *args)
{
    context = CTX_HUBLIST;
    return 0;
}

/* FIXME: allow switching between different hubs */
int func_hub(sp_t *sp, arg_t *args)
{
    if(current_hub)
        context = CTX_HUB;
    else
        printf("no current hub\n");
    return 0;
}

int func_ls(sp_t *sp, arg_t *args)
{
    int i = 0;

    sphub_t *hub;
    LIST_FOREACH(hub, &hubs, link)
    {
        printf("%02d: %s - %s\n", ++i, hub->address, hub->name);
    }
    return 0;
}

int func_cancel_transfer(sp_t *sp, arg_t *args)
{
    sp_send_cancel_transfer(sp, args->argv[1]);
    return 0;
}

int func_set_port(sp_t *sp, arg_t *args)
{
    sp_send_set_port(sp, strtoull(args->argv[1], NULL, 10));
    return 0;
}

int func_add_shared_path(sp_t *sp, arg_t *args)
{
    sp_send_add_shared_path(sp, args->argv[1]);
    return 0;
}

int func_remove_shared_path(sp_t *sp, arg_t *args)
{
    sp_send_remove_shared_path(sp, args->argv[1]);
    return 0;
}

int func_help(sp_t *sp, arg_t *args)
{
    int i;
    for(i = 1; i < args->argc; i++)
    {
        ui_cmd_t *cmd = get_cmd(args->argv[i]);
        if(cmd)
        {
            if(cmd == (ui_cmd_t *)-1)
                msg("%s: ambiguous command", args->argv[i]);
            else
                msg("%s: %s", cmd->name,
                        cmd->description ? cmd->description : "DON'T PANIC!");
        }
        else
            msg("%s: unknown command", args->argv[i]);
    }
    return 0;
}

int func_info(sp_t *sp, arg_t *args)
{
    msg("sharing %s:", str_size_human(total_share_size));

    shared_path_t *spath;
    LIST_FOREACH(spath, &shared_paths, link)
    {
        msg("  %-40s %s in %u files (%d%% hashed, %u files unhashed)",
                spath->path, str_size_human(spath->size), spath->nfiles,
                100 * spath->nfiles / (spath->ntotfiles ? spath->ntotfiles : 1),
                spath->ntotfiles - spath->nfiles);
    }
    return 0;
}

int func_set_passive(sp_t *sp, arg_t *args)
{
    passive_mode = !passive_mode;
    msg("setting passive mode %s", passive_mode ? "on" : "off");
    sp_send_set_passive(sp, passive_mode);
    return 0;
}

