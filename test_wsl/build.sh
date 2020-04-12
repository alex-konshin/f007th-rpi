#!/bin/sh

RUNDIR=`pwd`
BUILD_HOME=`dirname $0`
BUILD_HOME=`cd $BUILD_HOME/..; pwd`

cd ${BUILD_HOME}/test_wsl
make test_decode
status=$?

exit $status



