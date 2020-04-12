#!/bin/sh

RUNDIR=`pwd`
BUILD_HOME=`dirname $0`
BUILD_HOME=`cd $BUILD_HOME; pwd`

cd ${BUILD_HOME}/f007th-ts-all
make f007th-send
status=$?
if [ "$status" = 0 ]; then cp f007th-send ${BUILD_HOME}/bin/; fi
cd ${BUILD_HOME}/bin

exit $status
