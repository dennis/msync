#/bin/sh

TESTNUM=0
TEST_OK_COUNT=0
TEST_FAIL_COUNT=0

MSYNC=../src/msync

test_section() {
	local title="$1" # IN

	echo -e "\n\e[35m** $title\e[0m\n"
}

test_title() {
	local title="$1" # IN
	local len=`echo $title|wc -c`
	local dots
	local str

	TESTNUM=`expr $TESTNUM + 1`

	dots=`expr 30 - $len`
	str=""
	while test $dots -gt 1 ; do
		dots=`expr $dots - 1`
		str=".$str"
	done

	printf "  \e[33mt%02d\e[0m ${title}${str}: " $TESTNUM
}

test_okfail() {
	local rc=$1 # IN
	if [ $rc -eq 0 ]; then
		echo -e "\e[32mOK\e[0m"
		TEST_OK_COUNT=`expr $TEST_OK_COUNT + 1`
	else
		echo -e "\e[31mFAIL\e[0m"
		TEST_FAIL_COUNT=`expr $TEST_FAIL_COUNT + 1`
	fi
}

test_failok() {
	local rc=$1 # IN
	if [ $rc -eq 0 ]; then
		echo -e "\e[31mFAIL\e[0m"
		TEST_FAIL_COUNT=`expr $TEST_FAIL_COUNT + 1`
	else
		echo -e "\e[32mOK\e[0m"
		TEST_OK_COUNT=`expr $TEST_OK_COUNT + 1`
	fi
}

test_setup() {
	mkdir dir1 && touch -t 200801010000 dir1
	echo "dir1/file1" > dir1/file1 && touch -t 200801010000 dir1/file1
	echo "dir1/file2" > dir1/file2 && touch -t 200801010000 dir1/file2
	echo "dir1/file3" > dir1/file3 && touch -t 200801010000 dir1/file3
	mkdir dir1/dir1                && touch -t 200801010000 dir1/dir1

	echo '         (__) 
         (oo) 
   /------\/ 
  / |    ||   
 *  /\---/\ 
    ~~   ~~   
...."Have you mooed today?"...
' >dir1/moo && touch -t 200701010000 dir1/moo
}

test_teardown() {
	rm -rf dir1
}

## 

test_setup

test -f $MSYNC || (echo "$MSYNC not found, aborting")
test -d dir1 || (echo "dir1 not found, aborting")

test_section "Self tests"

	test_title "sanity-test"; 
		cd asdfad 2>/dev/null
		test_failok $?

	test_title "sanity-test";
		echo -n
		test_okfail $?

test_section "Slave tests"

	test_title "invalid hello";
		echo -e "HELLO msyncx 1\\n" | $MSYNC -s dir1 | grep "ERROR Protocol violation"  >/dev/null
		test_okfail $?

	test_title "invalid version"
		echo -e "HELLO msync 8888\n" | $MSYNC -s dir1 | grep "ERROR Protocol not supported" >/dev/null
		test_okfail $?

	test_title "correct hello"
		echo -e "HELLO msync 1\n" | $MSYNC -s dir1 | grep "ERROR Protocol not supported" >/dev/null
		test_failok $?

	test_title "gettime"
		echo -e "HELLO msync 1\nGETTIME" | $MSYNC -s dir1 | grep "GETTIME" >/dev/null 
		test_okfail $?

	test_title "filets"
		echo -e "HELLO msync 1\nFILETS ." | $MSYNC -s dir1 | grep "FILETS 1199142000" >/dev/null 
		test_okfail $?

	test_title "newerthan-1"
		C=`echo -e "HELLO msync 1\nNEWERTHAN 1199142000 ." | $MSYNC -s dir1 | wc -l`
		expr $C = 2 >/dev/null
		test_okfail $?

	test_title "newerthan-2"
		C=`echo -e "HELLO msync 1\nNEWERTHAN 1199141999 ." | ../src/msync -s dir1 | wc -l`
		expr $C = 6 >/dev/null
		test_okfail $?

	test_title "mkdir-1"
		echo -e "HELLO msync 1\nMKDIR -rwxr--r-- 1199141999 1199141999 1199141999 new-dir" | ../src/msync -s dir1 | grep "MKDIR new-dir" >/dev/null
		test_okfail $?

	test_title "mkdir-2"
		expr `stat --printf "%Y" dir1/new-dir` = 1199141999 >/dev/null
		test_okfail $?

	test_title "mkdir-3"
		echo -e "HELLO msync 1\nMKDIR -rwxr-xr-x 1199141999 1199141999 1199141999 new-dir" | ../src/msync -s dir1 | grep "MKDIR new-dir" >/dev/null 
		test_okfail $?

	test_title "mkdir-4"
		expr `stat --printf "%A" dir1/new-dir` = 'drwxr-xr-x' >/dev/null
		test_okfail $?

	rmdir dir1/new-dir

	test_title "get-1"
		echo -e "HELLO msync 1\nGET file1" | ../src/msync -s dir1 | \
			grep "PUT 11 0de32d6332a8f69f3a3f66cefe8923ac -rw-r--r-- 1199142000" | grep "1199142000 file1" >/dev/null
		test_okfail $?

	test_title "get-2"
		echo -e "HELLO msync 1\nGET file1" | ../src/msync -s dir1 | grep "dir1/file1" >/dev/null 
		test_okfail $?
	
	test_title "put-1"
		echo -e "HELLO msync 1\nPUT 0 d41d8cd98f00b204e9800998ecf8427e -r--r--r-- 1199141999 1199141999 1199141999 newfile" | ../src/msync -s dir1 | grep "GET newfile" >/dev/null 
		test_okfail $?

	test_title "put-2"
		expr `stat --printf "%A" dir1/newfile` = '-r--r--r--' >/dev/null
		test_okfail $?

	test_title "put-3"
		expr `stat --printf "%Y" dir1/newfile` = 1199141999 >/dev/null
		test_okfail $?

	rm -f dir1/newfile

	test_title "put-4"
		echo -e "HELLO msync 1\nPUT 6 3858f62230ac3c915f300c664312c63f -r--r--r-- 1199141999 1199141999 1199141999 newfile\nfoobar" | ../src/msync -s dir1 | grep "GET newfile"  >/dev/null
		test_okfail $?

	test_title "put-5"
		grep "foobar" dir1/newfile >/dev/null
		test_okfail $?

	rm -f dir1/newfile

	test_title "get-3"
		echo -e "HELLO msync 1\nGET dir1" | ../src/msync -s dir1 | egrep "MKDIR -rwxr-xr-x [0-9]+ [0-9]+ 1199142000" >/dev/null
		test_okfail $?

test_section "master tests"
TESTNUM=49
	test_title "sync-1"
		mkdir dir2
		../src/msync dir1 dir2 >/dev/null
		(cd dir1 && ls -l . >/tmp/msync-t51-dir1.txt)
		(cd dir2 && ls -l . >/tmp/msync-t51-dir2.txt)
		diff /tmp/msync-t51-dir?.txt >/dev/null
		test_okfail $?
		rm -r dir2
		rm /tmp/msync-t51-*

TESTNUM=95
test_section "status"
	test_title "Test OK"; echo $TEST_OK_COUNT
	test_title "Test FAIL"; echo $TEST_FAIL_COUNT


test_teardown
