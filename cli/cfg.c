/*
 * Copyright 2005 Martin Hedenfalk <martin@bzero.se>
 *
 * This file is part of ShakesPeer.
 *
 * ShakesPeer is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * ShakesPeer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with ShakesPeer; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */                                                                                      

#include "shakespeer.h"
#include "cfg.h"

/* for compatibility with libConfuse 2.4 */
#ifndef CFGF_NODEFAULT
# define CFGF_NODEFAULT CFGF_NONE
#endif

static int validate_log_level(cfg_t *cfg, cfg_opt_t *opt)
{
    if(strcasecmp(cfg_opt_getnstr(opt, 0), "none") == 0 ||
       strcasecmp(cfg_opt_getnstr(opt, 0), "warning") == 0 ||
       strcasecmp(cfg_opt_getnstr(opt, 0), "message") == 0 ||
       strcasecmp(cfg_opt_getnstr(opt, 0), "info") == 0 ||
       strcasecmp(cfg_opt_getnstr(opt, 0), "debug") == 0)
    {
        return 0;
    }

    cfg_error(cfg, "invalid log-level '%s'\n"
            "valid arguments are 'none', 'warning', 'message', 'info' or 'debug'",
            cfg_opt_getnstr(opt, 0));
    return -1;
}

cfg_t *parse_config(void)
{
    char *filename = "~/.shakespeer.conf";
    cfg_opt_t bookmark_opts[] = {
        CFG_STR("hub-address", 0, CFGF_NODEFAULT),
        CFG_STR("nick", 0, CFGF_NODEFAULT),
        CFG_END()
    };

    cfg_opt_t opts[] = {
        CFG_STR("default-nick", 0, CFGF_NODEFAULT),
        CFG_INT("port", 1412, CFGF_NONE),
        CFG_STR("log-level", "message", CFGF_NONE),
        CFG_SEC("bookmark", bookmark_opts, CFGF_MULTI | CFGF_TITLE),
        CFG_STR_LIST("shared-paths", 0, CFGF_NODEFAULT),
        CFG_STR("connection", "LAN(T3)", CFGF_NONE),
        CFG_STR("description", "", CFGF_NONE),
        CFG_STR("email", "", CFGF_NONE),
        CFG_STR("hublist-address", "http://www.hublist.org/PublicHubList.xml.bz2", CFGF_NONE),
#ifdef __APPLE__
        CFG_STR("download-directory", "~/Desktop/ShakesPeer Downloads", CFGF_NONE),
        CFG_STR("working-directory", "~/Library/Application Support/ShakesPeer", CFGF_NONE),
#else
        CFG_STR("download-directory", "~/shakespeer_downloads", CFGF_NONE),
        CFG_STR("working-directory", "~/.shakespeer", CFGF_NONE),
#endif
        CFG_STR("external-variables-address", "http://shakespeer.bzero.se/ip.shtml", CFGF_NONE),
        CFG_STR("sphubd-executable-path", "/usr/local/bin/sphubd", CFGF_NONE),
        CFG_STR("info-color", "\e[34m", CFGF_NONE),
        CFG_STR("info2-color", "\e[34m", CFGF_NONE),
        CFG_STR("status-color", "\e[35m", CFGF_NONE),
        CFG_STR("warning-color", "\e[31m", CFGF_NONE),
        CFG_STR("normal-color", "\e[0m", CFGF_NONE),
        CFG_BOOL("passive", cfg_false, CFGF_NONE),
        CFG_END()
    };

    cfg_t *xcfg = cfg_init(opts, CFGF_NOCASE);
    cfg_set_validate_func(xcfg, "log-level", validate_log_level);
    if(cfg_parse(xcfg, filename) == CFG_PARSE_ERROR)
        return NULL;
    return xcfg;
}

