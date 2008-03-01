#/bin/sh
mkdir dir1 && touch -t 200801010000 dir1
echo "dir1/file1" > dir1/file1 && touch -t 200801010000 dir1/file1
echo "dir1/file2" > dir1/file2 && touch -t 200801010000 dir1/file2
echo "dir1/file3" > dir1/file3 && touch -t 200801010000 dir1/file3
mkdir dir1/dir1                && touch -t 200801010000 dir1/dir1
