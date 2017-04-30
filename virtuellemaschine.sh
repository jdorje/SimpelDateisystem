#!/bin/sh

echo "password is DisBisHis2"
rsync -r -v -e "ssh -l csuser" . classvm58.cs.rutgers.edu:/home/csuser/proj3/
