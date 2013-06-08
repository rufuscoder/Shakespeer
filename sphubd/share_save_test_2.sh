#!/bin/sh

diff /tmp/share_save_test_2/MyList - <<EOF
tmp
	folder
		prout
			this_file_should_be_in_folder_prout_folder|30517
	folder 1
		this_file_should_be_in_folder_1_folder|8847
EOF
if test "$?" != "0"; then
    exit 1
else
    echo "generated /tmp/share_save_test_2/MyList is correct"
fi

diff /tmp/share_save_test_2/files.xml - <<EOF
<?xml version="1.0" encoding="utf-8" standalone="yes"?>
<FileListing Version="1" CID="HAEK3YLCADGFS" Base="/" Generator="DC++ 0.674">
<Directory Name="tmp">
	<Directory Name="folder">
		<Directory Name="prout">
			<File Name="this_file_should_be_in_folder_prout_folder" Size="30517" TTH="3XMIAVV5TOEY7DUCKZIKB3GGNRX2KSWD4ER5MPA"/>
		</Directory>
	</Directory>
	<Directory Name="folder 1">
		<File Name="this_file_should_be_in_folder_1_folder" Size="8847" TTH="4GJT555RBUVWSURTRMA5BKVAMTLF5O3JOZR23SI"/>
	</Directory>
</Directory>
</FileListing>
EOF
if test "$?" != "0"; then
    exit 1
else
    echo "generated /tmp/share_save_test_2/files.xml is correct"
fi

bzcat /tmp/share_save_test_2/files.xml.bz2 | diff /tmp/share_save_test_2/files.xml -
if test "$?" != "0"; then
    echo "bzip2-compressed version of files.xml differs from uncompressed"
    exit 1
else
    echo "bzip2-compressed version of files.xml matches uncompressed"
fi

