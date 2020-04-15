#!/bin/bash

set -e

if ! [[ -d gawk-4.2.1 ]]; then
	# Retrive and configure gawk
	wget http://ftp.gnu.org/gnu/gawk/gawk-4.2.1.tar.gz 
	gunzip gawk-4.2.1.tar.gz 
	tar -xvf gawk-4.2.1.tar 
	rm gawk-4.2.1.tar 
	cd gawk-4.2.1 
	./configure 
	cp ../list_malloc.c .
	cp ../list_malloc.h .
	cp ../buddy_malloc.c .
	cp ../buddy_malloc.h .
else
	cd gawk-4.2.1
fi

# Make check with list_malloc
sed -i 's/version\.\$(OBJEXT)/list_malloc\.\$(OBJEXT) version\.\$(OBJEXT)/' Makefile 
sed -i 's/version\.c/list_malloc\.c \\\n	version\.c/' Makefile
make check
sleep 5 # So we can see ALL TEST PASSED

# Make check with buddy_malloc
sed -i 's/list_malloc/buddy_malloc/' Makefile 
make check

# Clean Makefile
sed -i 's/buddy_malloc\.\$(OBJEXT) //' Makefile 
sed -i '/buddy_malloc/d' Makefile 
