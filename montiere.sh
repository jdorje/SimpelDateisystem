#!/bin/sh

ARGS="$*"
PATH_TO_MONTIERE="`dirname $0`"
NAME_OF_MONTIERE="`basename $0`"
MONTIERE_SH="$PATH_TO_MONTIERE/$NAME_OF_MONTIERE"

usage() {
	echo "Usage: $MONTIERE_SH [-m|--mount] [-u|--unmount] [-c|--check] "
	return 0
}

mount() {
	SFSBIN="$PATH_TO_MONTIERE/src/sfs"
	TOMOUNT="$PATH_TO_MONTIERE/output.file"
	MOUNTDIR="$PATH_TO_MONTIERE/example/mountdir"

	${SFSBIN} -d -f -s ${TOMOUNT} ${MOUNTDIR}
}

unmount() {
	fusermount -u example/mountdir
	echo "unmounted"
}

check() {
	mount | grep sfs
}

option="${1}"
case ${option} in
	-[mM]) mount ;;
	--mount) mount ;;
	-[uU]) unmount ;;
	--unmount) unmount ;;
	-[cC]) check ;;
	--count) check ;;
	*) usage ;;
esac
