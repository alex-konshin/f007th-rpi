#!/bin/bash

SCRIPT_HOME=`dirname $0`
SCRIPT_HOME=`cd $SCRIPT_HOME; pwd`
echo "\$SCRIPT_HOME=$SCRIPT_HOME"
pushd "$SCRIPT_HOME"
make all
status=$?
popd
if [ "$status" = 0 ]; then
  mkdir -p ${HOME}/bin/ 
  cp $SCRIPT_HOME/f007th-send ${HOME}/bin/
  if [ $? = 0 ]; then
    cd ${HOME}/bin
    echo "Executable file \"${HOME}/bin/f007th-send\" has been created."
  else
    echo "Error on copying file \"$SCRIPT_HOME/f007th-send\" to \"${HOME}/bin\" ."
  fi
fi

exit $status



