#/bin/bash
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

#COLORS
C_RESET="\033[0m"
C_PURPLE="\033[35m"
C_YELLOW="\033[33m"
C_GREEN="\033[32m"
C_RED="\033[31m"

DMSYNC=`pwd`/../src/dmsync
ECHO=echo
UNAME=`uname`
R=0 # return value
export TZ="Europe/Copenhagen"

test_section() {
	local title="$1" # IN

	$ECHO "\n${C_PURPLE}** $title${C_RESET}\n"
}

test_title() {
	local title="$1" # IN
	local len=`$ECHO $title|wc -c`
	local dots
	local str

	TESTNUM=`expr $TESTNUM + 1`

	dots=`expr 30 - $len`
	str=""
	while test $dots -gt 1 ; do
		dots=`expr $dots - 1`
		str=".$str"
	done

	printf "  ${C_YELLOW}t%02d${C_RESET} ${title}${str}: " $TESTNUM
}

sync_dirdiff() {
	local src="$1"
	local dst="$2"
	local msttmp="/tmp/dmsync-${TESTNUM}-mst.txt"

	mkdir $dst
	$DMSYNC $src $dst >$msttmp

	dirdiff $src $dst

	rm -r $dst
	rm $msttmp
}

dirdiff() {
	local a="$1"
	local b="$2"
	local atmp="/tmp/dmsync-${TESTNUM}-src.txt"
	local btmp="/tmp/dmsync-${TESTNUM}-dst.txt"

	(cd $a && ls -lR . >$atmp)
	(cd $b && ls -lR . >$btmp)
	diff $a $b >/dev/null
	test_okfail $?
	rm $atmp $btmp
}

mtime() {
	local file="$1"

	R=0

	case "$UNAME" in
		FreeBSD)
			R=`stat -r $file | cut -d " " -f10`
			;;
		*)
			R=`stat --printf "%Y" $file`
			;;
	esac
}

filemode() {
	local file="$1"

	R="??????????"

	case "$UNAME" in
		FreeBSD)
			R=`stat $file |cut  -d " " -f 3`
			;;
		*)
			R=`stat --printf "%A" $file`
			;;
	esac
}

inode() {
	local file="$1"

	R=0

	case "$UNAME" in
		FreeBSD)
			R=`stat -r $file | cut -d " " -f2`
			;;
		*)
			R=`stat --printf "%i" $file`
			;;
	esac
}

test_okfail() {
	local rc=$1 # IN
	if [ $rc -eq 0 ]; then
		$ECHO "${C_GREEN}OK${C_RESET}"
		TEST_OK_COUNT=`expr $TEST_OK_COUNT + 1`
	else
		$ECHO "${C_RED}FAIL${C_RESET}"
		TEST_FAIL_COUNT=`expr $TEST_FAIL_COUNT + 1`
	fi
}

test_failok() {
	local rc=$1 # IN
	if [ $rc -eq 0 ]; then
		$ECHO "${C_RED}FAIL${C_RESET}"
		TEST_FAIL_COUNT=`expr $TEST_FAIL_COUNT + 1`
	else
		$ECHO "${C_GREEN}OK${C_RESET}"
		TEST_OK_COUNT=`expr $TEST_OK_COUNT + 1`
	fi
}

test_setup() {
	mkdir dir1 && touch -t 200801010000 dir1
	$ECHO "dir1/file1" > dir1/file1 && touch -t 200801010000 dir1/file1
	$ECHO "dir1/file2" > dir1/file2 && touch -t 200801010000 dir1/file2
	$ECHO "dir1/file3" > dir1/file3 && touch -t 200801010000 dir1/file3
	mkdir dir1/dir1                && touch -t 200801010000 dir1/dir1

	$ECHO '         (__) 
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
	rm -rf dir1 dir2 dir3 dir4 dir5 2>/dev/null
}

## 

if test ! -f $DMSYNC ; then
	$ECHO "$DMSYNC not found, aborting"
	exit
fi

test_setup

if test ! -d dir1 ; then 
	$ECHO "dir1 not found, aborting";
	exit
fi


test_section "Self tests"

	test_title "sanity-test"; 
		cd asdfad 2>/dev/null
		test_failok $?

	test_title "sanity-test";
		$ECHO -n
		test_okfail $?

	test_title "timezone-test"
		touch -t 200801010000 TZ_TEST
		mtime "TZ_TEST"
		expr $R = 1199142000 >/dev/null
		test_okfail $?
		rm TZ_TEST


expr $TEST_FAIL_COUNT = 0 >/dev/null || exit

test_section "Slave tests"

	test_title "invalid hello";
		$ECHO "HELLO msyncx 1" | $DMSYNC -s dir1 | grep "ERROR Protocol violation"  >/dev/null
		test_okfail $?

	test_title "invalid version"
		$ECHO "HELLO dmsync 8888" | $DMSYNC -s dir1 | grep "ERROR Protocol not supported" >/dev/null
		test_okfail $?

	test_title "correct hello"
		$ECHO "HELLO dmsync 1" | $DMSYNC -s dir1 | grep "ERROR Protocol not supported" >/dev/null
		test_failok $?

	test_title "gettime"
		$ECHO "HELLO dmsync 1\nGETTIME" | $DMSYNC -s dir1 | grep "GETTIME" >/dev/null 
		test_okfail $?

	test_title "scan"
		$ECHO "HELLO dmsync 1\nSCAN ." | $DMSYNC -s dir1 | grep "SCAN 1199142000" >/dev/null 
		test_okfail $?

	test_title "newerthan-1"
		C=`$ECHO "HELLO dmsync 1\nNEWERTHAN 1199142000 ." | $DMSYNC -s dir1 | egrep "^(FILE|DIR|HLNK)" | wc -l`
		expr $C = 0 >/dev/null
		test_okfail $?

	test_title "newerthan-2"
		C=`$ECHO "HELLO dmsync 1\nNEWERTHAN 1199141999 ." | $DMSYNC -s dir1 | egrep "^(FILE|DIR|HLNK)" | wc -l`
		expr $C = 4 >/dev/null
		test_okfail $?

	test_title "mkdir-1"
		$ECHO "HELLO dmsync 1\nMKDIR -rwxr--r-- 1199141999 1199141999 1199141999 new-dir" | $DMSYNC -s dir1 | grep "MKDIR new-dir" >/dev/null
		test_okfail $?

	test_title "mkdir-2"
		mtime "dir1/new-dir"
		expr $R = 1199141999 >/dev/null
		test_okfail $?

	test_title "mkdir-3"
		$ECHO "HELLO dmsync 1\nMKDIR -rwxr-xr-x 1199141999 1199141999 1199141999 new-dir" | $DMSYNC -s dir1 | grep "MKDIR new-dir" >/dev/null 
		test_okfail $?

	test_title "mkdir-4"
		filemode "dir1/new-dir"
		expr "x$R" = 'xdrwxr-xr-x' >/dev/null
		test_okfail $?

	rmdir dir1/new-dir

	test_title "get-1"
		$ECHO "HELLO dmsync 1\nGET file1" | $DMSYNC -s dir1 | \
			grep "PUT 11 0de32d6332a8f69f3a3f66cefe8923ac -rw-r--r-- 1199142000" | grep "1199142000 file1" >/dev/null
		test_okfail $?

	test_title "get-2"
		$ECHO "HELLO dmsync 1\nGET file1" | $DMSYNC -s dir1 | grep "dir1/file1" >/dev/null 
		test_okfail $?
	
	test_title "put-1"
		$ECHO "HELLO dmsync 1\nPUT 0 d41d8cd98f00b204e9800998ecf8427e -r--r--r-- 1199141999 1199141999 1199141999 newfile" | $DMSYNC -s dir1 | grep "GET newfile" >/dev/null 
		test_okfail $?

	test_title "put-2"
		filemode "dir1/newfile"
		expr "x$R" = 'x-r--r--r--' >/dev/null
		test_okfail $?

	test_title "put-3"
		mtime "dir1/newfile"
		expr $R = 1199141999 >/dev/null
		test_okfail $?

	rm -f dir1/newfile

	test_title "put-4"
		$ECHO "HELLO dmsync 1\nPUT 6 3858f62230ac3c915f300c664312c63f -r--r--r-- 1199141999 1199141999 1199141999 newfile\nfoobar" | $DMSYNC -s dir1 | grep "GET newfile"  >/dev/null
		test_okfail $?

	test_title "put-5"
		grep "foobar" dir1/newfile >/dev/null
		test_okfail $?

	rm -f dir1/newfile

	test_title "get-3"
		$ECHO "HELLO dmsync 1\nGET dir1" | $DMSYNC -s dir1 | egrep "MKDIR -rwxr-xr-x [0-9]+ [0-9]+ 1199142000" >/dev/null
		test_okfail $?
	
	test_title "symlink-1"
		$ECHO "HELLO dmsync 1\nGET symlink" | $DMSYNC -s dir3 | egrep "SLNK symlink" >/dev/null
		test_okfail $?

	test_title "symlink-2"
		mkdir testdir
		touch testdir/normal
		$ECHO "HELLO dmsync 1\nSLNK symlink\nnormal" | $DMSYNC -s testdir >/dev/null
		test -h testdir/symlink
		test_okfail $?
		rm -rf testdir
	
	test_title "hardlink-1"
		$ECHO "HELLO dmsync 1\nNEWERTHAN 0 ." | $DMSYNC -s dir4 | grep "HLNK ./hardlink" >/dev/null
		test_okfail $?
	
	test_title "hardlink-2"
		$ECHO "HELLO dmsync 1\nGET ./hardlink" | $DMSYNC -s dir4 |grep -v "HLNK ./hardlink" >/dev/null
		test_okfail $?
	
	test_title "hardlink-3"
		$ECHO "HELLO dmsync 1\nHLNK ./hardlink\n./hardlink\n" | $DMSYNC -s . |grep "ERROR Cannot make a link to itself!" >/dev/null
		test_okfail $?

	test_title "hardlink-4"
		mkdir testdir
		touch testdir/file
		$ECHO "HELLO dmsync 1\nHLNK ./hardlink\n./file\n" | $DMSYNC -s testdir/ |grep "ERROR Cannot make a link to itself!" >/dev/null
		inode "testdir/file"; R1=$R
		inode "testdir/hardlink"; R2=$R
		expr $R1 = $R2 >/dev/null
		test_okfail $?
		rm -rf testdir
	
	test_title "exists-1"
		$ECHO "HELLO dmsync 1\nEXISTS ./hardlink" |  $DMSYNC -s dir4 | grep "YES" >/dev/null
		test_okfail $?

	test_title "exists-2"
		$ECHO "HELLO dmsync 1\nEXISTS ./hardlink-nogo" |  $DMSYNC -s dir4 | grep "YES" >/dev/null
		test_failok $?

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
		sync_dirdiff "dir5" "dir5-copy"

	test_title "sync-6 (using -S/-D)"
		mkdir dir5-copy
		$DMSYNC -S "$DMSYNC -s dir5" -D "$DMSYNC -s dir5-copy" >/dev/null
		dirdiff dir5 dir5-copy
		rm -r dir5-copy
	
	test_title "sync-7 (using ssh)"
		mkdir dir5-copy
		$DMSYNC -S "$DMSYNC -s dir5" -D "`which ssh` localhost $DMSYNC -s `pwd`/dir5-copy" >/dev/null
		dirdiff dir5 dir5-copy
		rm -r dir5-copy

TESTNUM=95
test_section "status"
	test_title "Test OK"; $ECHO $TEST_OK_COUNT
	test_title "Test FAIL"; $ECHO $TEST_FAIL_COUNT

test_teardown
