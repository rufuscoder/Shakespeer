#!/bin/sh

diff /tmp/MyList - <<EOF
tmp
	test1
	test2
		test2sub1
			test2sub1file1|12345
			test2sub1file2|876334232
		test2sub2
		test2sub3
	test3
		test3file3|342345
		test3file4|743334232
		test3sub1
		test3sub2
		test3sub3
			test4sub2
			test4sub3
			test4sub1
EOF
if test "$?" != "0"; then
    exit 1
else
    echo "generated /tmp/MyList is correct"
fi

diff /tmp/files.xml - <<EOF
<?xml version="1.0" encoding="utf-8" standalone="yes"?>
<FileListing Version="1" CID="HAEK3YLCADGFS" Base="/" Generator="DC++ 0.674">
<Directory Name="tmp">
	<Directory Name="test1">
	</Directory>
	<Directory Name="test2">
		<Directory Name="test2sub1">
			<File Name="test2sub1file1" Size="12345" TTH="AJFKIBU7RMNS3JKGPL1QWEAU4HXCZU55Y3HJSDY"/>
			<File Name="test2sub1file2" Size="876334232" TTH="GSWWMC3J2KLJHFOPWMNFUZAQWSKAJSFFGHWNMX7"/>
		</Directory>
		<Directory Name="test2sub2">
		</Directory>
		<Directory Name="test2sub3">
		</Directory>
	</Directory>
	<Directory Name="test3">
		<File Name="test3file3" Size="342345" TTH="AJFGJSKKFJSWMANBNSD76JAU4HXCZU55Y3HJSDY"/>
		<File Name="test3file4" Size="743334232" TTH="SJKFJU7J2KLJHFOPWMNFUZAQWSKAJSFFGHWNMX7"/>
		<Directory Name="test3sub1">
		</Directory>
		<Directory Name="test3sub2">
		</Directory>
		<Directory Name="test3sub3">
			<Directory Name="test4sub2">
			</Directory>
			<Directory Name="test4sub3">
			</Directory>
			<Directory Name="test4sub1">
			</Directory>
		</Directory>
	</Directory>
</Directory>
</FileListing>
EOF
if test "$?" != "0"; then
    exit 1
else
    echo "generated /tmp/files.xml is correct"
fi

bzcat /tmp/files.xml.bz2 | diff /tmp/files.xml -
if test "$?" != "0"; then
    echo "bzip2-compressed version of files.xml differs from uncompressed"
    exit 1
else
    echo "bzip2-compressed version of files.xml matches uncompressed"
fi

