#/bin/sh
# The MIT License
# 
# Copyright (c) 2008 Dennis Møllegaard Pedersen <dennis@moellegaard.dk>
# 
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
# 
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
# 
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.

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

sync_dirdiff() {
	local src="$1"
	local dst="$2"
	local srctmp="/tmp/msync-${TESTNUM}-1.txt"
	local dsttmp="/tmp/msync-${TESTNUM}-2.txt"
	local r=0

	mkdir $dst
	$MSYNC $src $dst >/dev/null
	(cd $src && ls -l . >$srctmp)
	(cd $dst && ls -l . >$dsttmp)
	diff $srctmp $dsttmp >/dev/null
	test_okfail $?
	rm -r $dst
	#rm $srctmp $dsttmp
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

	mkdir -p dir2/dir1/dir2/dir3/dir4/dir5/dir6/dir7/dir8/dir9/dir10/dir11/dir12/dir13/dir14/dir15/dir16/dir17/dir18/dir19

	mkdir dir3
	touch -t 200801010000 dir3/normal 
	(cd dir3 && ln -s normal symlink)
	mkdir dir3/dir1
	(cd dir3/dir1 && ln -s ../normal symlink2)

	mkdir dir4
	touch dir4/normal
	(cd dir4 && ln normal hardlink)
	mkdir dir4/dir1
	(cd dir4/dir1 && ln ../normal hardlink2)

	mkdir dir5
	touch dir5/normal
	(cd dir5 && ln -s ../tests.sh outsider.sh)
}

test_teardown() {
	rm -rf dir1 dir2 dir3 dir4 dir5
}

## 

if test ! -f $MSYNC ; then
	echo "$MSYNC not found, aborting"
	exit
fi

test_setup

if test ! -d dir1 ; then 
	echo "dir1 not found, aborting";
	exit
fi


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
		echo -e "HELLO msync 1\nSCAN ." | $MSYNC -s dir1 | grep "SCAN 1199142000" >/dev/null 
		test_okfail $?

	test_title "newerthan-1"
		C=`echo -e "HELLO msync 1\nNEWERTHAN 1199142000 ." | $MSYNC -s dir1 | wc -l`
		expr $C = 2 >/dev/null
		test_okfail $?

	test_title "newerthan-2"
		C=`echo -e "HELLO msync 1\nNEWERTHAN 1199141999 ." | $MSYNC -s dir1 | wc -l`
		expr $C = 6 >/dev/null
		test_okfail $?

	test_title "mkdir-1"
		echo -e "HELLO msync 1\nMKDIR -rwxr--r-- 1199141999 1199141999 1199141999 new-dir" | $MSYNC -s dir1 | grep "MKDIR new-dir" >/dev/null
		test_okfail $?

	test_title "mkdir-2"
		expr `stat --printf "%Y" dir1/new-dir` = 1199141999 >/dev/null
		test_okfail $?

	test_title "mkdir-3"
		echo -e "HELLO msync 1\nMKDIR -rwxr-xr-x 1199141999 1199141999 1199141999 new-dir" | $MSYNC -s dir1 | grep "MKDIR new-dir" >/dev/null 
		test_okfail $?

	test_title "mkdir-4"
		expr `stat --printf "%A" dir1/new-dir` = 'drwxr-xr-x' >/dev/null
		test_okfail $?

	rmdir dir1/new-dir

	test_title "get-1"
		echo -e "HELLO msync 1\nGET file1" | $MSYNC -s dir1 | \
			grep "PUT 11 0de32d6332a8f69f3a3f66cefe8923ac -rw-r--r-- 1199142000" | grep "1199142000 file1" >/dev/null
		test_okfail $?

	test_title "get-2"
		echo -e "HELLO msync 1\nGET file1" | $MSYNC -s dir1 | grep "dir1/file1" >/dev/null 
		test_okfail $?
	
	test_title "put-1"
		echo -e "HELLO msync 1\nPUT 0 d41d8cd98f00b204e9800998ecf8427e -r--r--r-- 1199141999 1199141999 1199141999 newfile" | $MSYNC -s dir1 | grep "GET newfile" >/dev/null 
		test_okfail $?

	test_title "put-2"
		expr `stat --printf "%A" dir1/newfile` = '-r--r--r--' >/dev/null
		test_okfail $?

	test_title "put-3"
		expr `stat --printf "%Y" dir1/newfile` = 1199141999 >/dev/null
		test_okfail $?

	rm -f dir1/newfile

	test_title "put-4"
		echo -e "HELLO msync 1\nPUT 6 3858f62230ac3c915f300c664312c63f -r--r--r-- 1199141999 1199141999 1199141999 newfile\nfoobar" | $MSYNC -s dir1 | grep "GET newfile"  >/dev/null
		test_okfail $?

	test_title "put-5"
		grep "foobar" dir1/newfile >/dev/null
		test_okfail $?

	rm -f dir1/newfile

	test_title "get-3"
		echo -e "HELLO msync 1\nGET dir1" | $MSYNC -s dir1 | egrep "MKDIR -rwxr-xr-x [0-9]+ [0-9]+ 1199142000" >/dev/null
		test_okfail $?
	
	test_title "symlink-1"
		echo -e "HELLO msync 1\nGET symlink" | $MSYNC -s dir3 | egrep "SLNK symlink" >/dev/null
		test_okfail $?

	test_title "symlink-2"
		mkdir testdir
		touch testdir/normal
		echo -e "HELLO msync 1\nSLNK symlink\nnormal" | $MSYNC -s testdir >/dev/null
		test -h testdir/symlink
		test_okfail $?
		rm -rf testdir
		

test_section "master tests"
TESTNUM=49
	test_title "sync-1 (simple)"
		sync_dirdiff "dir1" "dir1-copy"

	test_title "sync-2 (many subdirs)"
		sync_dirdiff "dir2" "dir2-copy"

	test_title "sync-3 (symlink)"
		sync_dirdiff "dir3" "dir3-copy"

	test_title "sync-4 (hardlink)"
		sync_dirdiff "dir4" "dir4-copy"

	test_title "sync-5 (symlinks)"
		mkdir dir5-copy
		$MSYNC dir5 dir5-copy >/dev/null
		C=`ls -l dir5-copy/ | wc -l`
		expr $C = 1 >/dev/null
		test_okfail $?
		rm -rf dir5-copy

TESTNUM=95
test_section "status"
	test_title "Test OK"; echo $TEST_OK_COUNT
	test_title "Test FAIL"; echo $TEST_FAIL_COUNT

test_teardown
