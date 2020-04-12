#!/bin/bash

# ONLY TO BE EXECUTED IN DOCKER CONTAINER
cd /root/gawk-4.2.1
sed -i 's/version\.\$(OBJEXT)/list_malloc\.\$(OBJEXT) version\.\$(OBJEXT)/' Makefile 
sed -i 's/version\.c/list_malloc\.c \\\n	version\.c/' Makefile
make check

