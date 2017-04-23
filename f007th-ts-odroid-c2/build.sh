#!/bin/bash

make f007th-send
status=$?
if [ "$status" == 0 ]; then
  mkdir -p ${HOME}/bin/ 
  cp f007th-send ${HOME}/bin/
  cd ${HOME}/bin
fi

exit $status



