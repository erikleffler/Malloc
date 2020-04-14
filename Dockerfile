FROM ubuntu:18.04
RUN apt-get update &&\
	apt-get install -y locales &&\
	rm -rf /var/lib/apt/lists/* &&\ 
	localedef -i en_US -c -f UTF-8 -A /usr/share/locale/locale.alias en_US.UTF-8
ENV LANG en_US.utf8
RUN apt update &&\
	apt install -y git wget build-essential libgmp3-dev libmpc-dev libreadline-dev vim gdb &&\
	cd /root &&\
	wget http://ftp.gnu.org/gnu/gawk/gawk-4.2.1.tar.gz &&\
	gunzip gawk-4.2.1.tar.gz &&\
	tar -xvf gawk-4.2.1.tar &&\
	rm gawk-4.2.1.tar &&\
	cd gawk-4.2.1 &&\
	./configure &&\
	make
# Do make only since it is nice to have it cached ^^^.
ADD ./list_malloc.c /root/gawk-4.2.1/list_malloc.c
ADD ./list_malloc.h /root/gawk-4.2.1/list_malloc.h
ADD ./buddy_malloc.c /root/gawk-4.2.1/buddy_malloc.c
ADD ./buddy_malloc.h /root/gawk-4.2.1/buddy_malloc.h
ADD ./check.sh /root/gawk-4.2.1/check.sh
