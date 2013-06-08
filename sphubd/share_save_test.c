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

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "share.h"
#include "unit_test.h"

typedef struct tdir tdir_t;
typedef struct tfile tfile_t;

struct tfile
{
    const char *name;
    const char *tth;
    guint64 size;
};

struct tdir
{
    const char *name;
    tdir_t *subdirs;
    tfile_t *files;
    int enabled;
};

tfile_t test2_sub1_files[] = {
    {"test2sub1file1", "AJFKIBU7RMNS3JKGPL1QWEAU4HXCZU55Y3HJSDY", 12345LL},
    {"test2sub1file2", "GSWWMC3J2KLJHFOPWMNFUZAQWSKAJSFFGHWNMX7", 876334232LL},
    {0, 0, 0}
};

tdir_t test2_subdir[] = {
    {"/test2/test2sub1", 0, test2_sub1_files, 1},
    {"/test2/test2sub2", 0, 0, 1},
    {"/test2/test2sub3", 0, 0, 1},
    {0, 0, 0, 0}
};

tdir_t test3_subdir[] = {
    {"/test3/test3sub1", 0, 0, 1},
    {"/test3/test3sub2", 0, 0, 1},
    {"/test3/test3sub3", 0, 0, 1},
    {0, 0, 0, 1}
};

tfile_t test3_files[] = {
    {"test3file3", "AJFGJSKKFJSWMANBNSD76JAU4HXCZU55Y3HJSDY", 342345LL},
    {"test3file4", "SJKFJU7J2KLJHFOPWMNFUZAQWSKAJSFFGHWNMX7", 743334232LL},
    {0, 0, 0}
};

tdir_t test4_subdir[] = {
    {"/test4/disables/test4sub1", 0, 0, 1},
    {"/test4/disabled/test4sub2", 0, 0, 1},
    {"/test4/disabled/test4sub3", 0, 0, 1},
    {0, 0, 0, 0}
};

tfile_t test4_files[] = {
    {"test4file5", "ASDF34GDFBTYKADGT35GGJAU4HXCZU55Y3HJSDY", 613435LL},
    {"test4file6", "SJKFJU7J2KLJHFOPWMGQFQQQCQQEGHEGXV322X7", 3451142LL},
    {0, 0, 0}
};

tdir_t directories[] = {
    {"/test1", 0, 0, 1},
    {"/test2", test2_subdir, 0, 1},
    {"/test3", test3_subdir, test3_files, 1},
    {"/test4/disabled", test4_subdir, test4_files, 0},
    {0, 0, 0, 0}
};


tfile_t folder_prout_files[] = {
    {"this_file_should_be_in_folder_prout_folder", "3XMIAVV5TOEY7DUCKZIKB3GGNRX2KSWD4ER5MPA", 30517LL},
    {0, 0, 0}
};

tdir_t folder_subdir[] = {
    {"/folder/prout", 0, folder_prout_files, 1},
    {0, 0, 0, 0}
};

tfile_t folder_1_files[] = {
    {"this_file_should_be_in_folder_1_folder", "4GJT555RBUVWSURTRMA5BKVAMTLF5O3JOZR23SI", 8847LL},
    {0, 0, 0}
};

tdir_t directories_2[] = {
    {"/folder", folder_subdir, 0, 1},
    {"/folder 1", 0, folder_1_files, 1},
    {0, 0, 0, 0}
};

void build_share(int64_t mount_id, tdir_t *dirs)
{
    int i;
    for(i = 0; dirs[i].name; i++)
    {
        int rc = db_exec_sql(NULL, "INSERT INTO directory (name, mount_id, enabled) VALUES ('%q', %lli, %i)",
                dirs[i].name, mount_id, dirs[i].enabled);
        fail_unless(rc == 0);
        if(dirs[i].files)
        {
            int64_t dir_id = db_last_insert_id();
            int j;
            for(j = 0; dirs[i].files[j].name; j++)
            {
                GError *err = 0;
                tfile_t *f = &dirs[i].files[j];
                rc = db_exec_sql(&err, "INSERT INTO file (name, tth, size, directory_id, type, mtime)"
                            " VALUES ('%q', '%q', %llu, %lli, %i, 0)",
                            f->name, f->tth, f->size, dir_id, share_filetype(f->name));
                if(err)
                    WARNING("%s", err->message);
                fail_unless(err == 0);
                fail_unless(rc == 0);
            }
        }
        if(dirs[i].subdirs)
        {
            build_share(mount_id, dirs[i].subdirs);
        }
    }
}

void test_share(const char *dir, tdir_t *dirs)
{
    mkpath(dir);
    
    fail_unless(db_init(NULL, NULL) == 0);
    share_t *share = share_new(dir);
    fail_unless(share);

    GError *err = 0;

    /* ensure we have a known, testable CID */
    free(share->cid);
    share->cid = strdup("HAEK3YLCADGFS");

    int rc = db_exec_sql(NULL, "INSERT INTO mount (path, virtual_root) VALUES ('%s', 'tmp')", dir);
    fail_unless(rc == 0);
    int64_t mount_id = db_last_insert_id();

    build_share(mount_id, dirs);

    fail_unless(share_save(share, &err, FILELIST_DCLST | FILELIST_XML) == 0);
    /* The generated file is check by share_save_test.sh */

    db_close();
}

int main(void)
{
    test_share("/tmp", directories);
    test_share("/tmp/share_save_test_2", directories_2);

    return 0;
}

