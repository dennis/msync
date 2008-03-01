#/bin/sh

echo "** Slave tests"
echo -n "  t01 invalid hello..: "
((echo -e "HELLO msyncx 1\n" | ../src/msync -s dir1 | grep "ERROR Protocol violation" >/dev/null ) && echo OK) || echo FAIL

echo -n "  t02 invalid hello..: "
((echo -e "HELLO msync 8888\n" | ../src/msync -s dir1 | grep "ERROR Protocol not supported" >/dev/null ) && echo OK) || echo FAIL

echo -n "  t03 correct hello..: "
((echo -e "HELLO msync 1\n" | ../src/msync -s dir1 | grep "ERROR Protocol not supported" >/dev/null ) && echo FAIL) || echo OK

echo -n "  t04 gettime........: "
((echo -e "HELLO msync 1\nGETTIME" | ../src/msync -s dir1 | grep "GETTIME" >/dev/null ) && echo OK) || echo FAIL

echo -n "  t05 filets.........: "
((echo -e "HELLO msync 1\nFILETS ." | ../src/msync -s dir1 | grep "FILETS 1199142000" >/dev/null ) && echo OK) || echo FAIL

echo -n "  t06 newerthan......: "
C=`echo -e "HELLO msync 1\nNEWERTHAN 1199142000 ." | ../src/msync -s dir1 | wc -l`
((expr $C = 2 >/dev/null) && echo OK) || echo FAIL

echo -n "  t07 newerthan......: "
C=`echo -e "HELLO msync 1\nNEWERTHAN 1199141999 ." | ../src/msync -s dir1 | wc -l`
((expr $C = 6 >/dev/null) && echo OK) || echo FAIL

echo -n "  t08 mkdir..........: "
((echo -e "HELLO msync 1\nMKDIR -rwxr--r-- 1199141999 1199141999 1199141999 new-dir" | ../src/msync -s dir1 | grep "MKDIR new-dir" >/dev/null ) && echo OK ) || echo FAIL

echo -n "  t09 mkdir..........: "
(expr `stat --printf "%Y" dir1/new-dir` = 1199141999 >/dev/null && echo OK) || echo FAIL

echo -n "  t10 mkdir..........: "
((echo -e "HELLO msync 1\nMKDIR -rwxr-xr-x 1199141999 1199141999 1199141999 new-dir" | ../src/msync -s dir1 | grep "MKDIR new-dir" >/dev/null ) && echo OK ) || echo FAIL

echo -n "  t11 mkdir..........: "
(expr `stat --printf "%A" dir1/new-dir` = 'drwxr-xr-x' >/dev/null && echo OK) || echo FAIL

rmdir dir1/new-dir

echo -n "  t12 get............: "
(echo -e "HELLO msync 1\nGET file1" | ../src/msync -s dir1 | \
	grep "PUT 11 0de32d6332a8f69f3a3f66cefe8923ac -rw-r--r-- 1199142000" | grep "1199142000 file1" >/dev/null && echo OK) || echo FAIL

echo -n "  t13 get............: "
(echo -e "HELLO msync 1\nGET file1" | ../src/msync -s dir1 | grep "dir1/file1" >/dev/null && echo OK) || echo FAIL

echo -n "  t14 put............: "
(echo -e "HELLO msync 1\nPUT 0 d41d8cd98f00b204e9800998ecf8427e -r--r--r-- 1199141999 1199141999 1199141999 newfile" | ../src/msync -s dir1 | grep "GET newfile" >/dev/null && echo OK) || echo FAIL

echo -n "  t15 put............: "
(expr `stat --printf "%A" dir1/newfile` = '-r--r--r--' >/dev/null && echo OK) || echo FAIL

echo -n "  t16 put............: "
(expr `stat --printf "%Y" dir1/newfile` = 1199141999 >/dev/null && echo OK) || echo FAIL

rm -f dir1/newfile

echo -n "  t17 put............: "
(echo -e "HELLO msync 1\nPUT 6 3858f62230ac3c915f300c664312c63f -r--r--r-- 1199141999 1199141999 1199141999 newfile\nfoobar" | ../src/msync -s dir1 | grep "GET newfile"  >/dev/null && echo OK) || echo FAIL

echo -n "  t18 put............: "
(grep "foobar" dir1/newfile >/dev/null && echo OK) || echo FAIL

rm -f dir1/newfile

echo -n "  t19 get (dir)......: "
(echo -e "HELLO msync 1\nGET dir1" | ../src/msync -s dir1 | grep "MKDIR -rwxr-xr-x 1199142000" >/dev/null && echo OK) || echo FAIL

echo "** master tests"

echo -n "  t50 sync...........: "
mkdir dir2
../src/msync dir1 dir2 >/dev/null
(diff dir1 dir2 >/dev/null && echo OK) || echo FAIL
rm -r dir2
