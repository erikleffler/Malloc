#!/bin/bash
docker build -t malloc_test .
docker run malloc_test bash /root/gawk-4.2.1/check.sh
