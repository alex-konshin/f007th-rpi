#!/bin/sh

RUNDIR=`pwd`
BUILD_HOME=`dirname $0`
BUILD_HOME=`cd $BUILD_HOME; pwd`

cd ${BUILD_HOME}/f007th-rpi
make f007th-rpi
status=$?
if [ "$status" != 0 ]; then exit $status; fi
if [ ! -d ${BUILD_HOME}/bin ]; then mkdir ${BUILD_HOME}/bin; fi
cp f007th-rpi ${BUILD_HOME}/bin/

cd ${BUILD_HOME}/f007th-rpi_send
make f007th-rpi_send
status=$?
if [ "$status" = 0 ]; then cp f007th-rpi_send ${BUILD_HOME}/bin/; fi

cd ${BUILD_HOME}/f007th-ts-rpi
make f007th-send
status=$?
if [ "$status" = 0 ]; then cp f007th-send ${BUILD_HOME}/bin/; fi
cd ${BUILD_HOME}/bin

exit $status



