FROM ubuntu:18.04
RUN apt-get update && apt-get install -y locales && rm -rf /var/lib/apt/lists/* \
    && localedef -i en_US -c -f UTF-8 -A /usr/share/locale/locale.alias en_US.UTF-8
ENV LANG en_US.utf8
RUN apt update
RUN apt install -y git wget build-essential libgmp3-dev libmpc-dev libreadline-dev
ADD "https://www.random.org/cgi-bin/randbyte?nbytes=10&format=h" skipcache 
RUN cd /root &&\
	git clone https://github.com/erikleffler/Malloc &&\
	cd Malloc &&\
	wget http://ftp.gnu.org/gnu/gawk/gawk-4.2.1.tar.gz &&\
	gunzip gawk-4.2.1.tar.gz &&\
	tar -xvf gawk-4.2.1.tar &&\
	rm gawk-4.2.1.tar &&\
	cd gawk-4.2.1 &&\
	./configure &&\
	sed -i '152s/$/ \.\.\/malloc.$(OBJEXT)/' Makefile &&\
	sed -i '525i\.\.\/malloc\.c \\' Makefile &&\
	make check

