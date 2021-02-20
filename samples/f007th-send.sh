#! /bin/sh
### BEGIN INIT INFO
# Provides: f007th-send
# Required-Start: $all
# Required-Stop: $all
# Default-Start: 2 3 4 5
# Default-Stop: 0 1 6
# Short-Description: f007th-send server
# Description: Receiver for 433MHz temperature sensors
### END INIT INFO

#---------------------------------------------
# Change if necessary:
PROG_NAME=f007th-send
CFG_FILE=f007th-send.cfg
BIN_PATH=/home/pi/f007th-rpi/bin
CFG_PATH=$BIN_PATH
RUNUSER=pi
#---------------------------------------------

SCRIPT_NAME=$(basename $0)

# currently ununsed: full path to script = path to program
#SCRIPT_PATH="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
#SCRIPT_PATH="$( cd "$( echo "${BASH_SOURCE[0]%/*}" )" && pwd )"
#BIN_PATH=$SCRIPT_PATH

 
case "$1" in
    start)
        # Start the program
        ps -e | grep -v grep | grep -v $SCRIPT_NAME | grep $PROG_NAME >/dev/null 2>&1
        if [ $? -eq 0 ]; then
            echo $PROG_NAME is already running
            exit 1
        fi
        echo "$PROG_NAME is starting as user: $RUNUSER"
        if [ "$RUNUSER" =  "$USER" ]; then
            $BIN_PATH/$PROG_NAME -c $CFG_PATH/$CFG_FILE >/dev/null 2>&1 &
        else
            runuser -l $RUNUSER -c "$BIN_PATH/$PROG_NAME -c $CFG_PATH/$CFG_FILE >/dev/null 2>&1 &"
        fi
        ;;
    stop)
        # Stop the program
        echo "$PROG_NAME is stopping"
        killall -SIGINT $PROG_NAME
        ;;
    restart)
        # Restart the program
        echo "$PROG_NAME is stopping"
        killall -SIGINT $PROG_NAME
        echo "$PROG_NAME is starting as user: $RUNUSER"
        if [ "$RUNUSER" = "$USER" ]; then
            $BIN_PATH/$PROG_NAME -c $CFG_PATH/$CFG_FILE >/dev/null 2>&1 &
        else
            runuser -l $RUNUSER -c "$BIN_PATH/$PROG_NAME -c $CFG_PATH/$CFG_FILE >/dev/null 2>&1 &"
        fi
        ;;
    status)
        ps -ef | grep -v grep | grep -v $SCRIPT_NAME | grep $PROG_NAME
        if [ $? -ne 0 ]; then
            echo $PROG_NAME is not running
        fi
        ;;
    remove)
	sudo update-rc.d -f $SCRIPT_NAME remove
        ;;
    install)
        if [ -f /etc/init.d/$SCRIPT_NAME ]; then
            echo "$SCRIPT_NAME is already installed"
            echo "Use: $SCRIPT_NAME reinstall"
            exit 1
        fi
        if [ ! -f $BIN_PATH/$PROG_NAME ]; then
            echo "$BIN_PATH/$PROG_NAME does not exists"
            exit 1
        fi
        sudo mkdir /var/log/$PROG_NAME 2>/dev/null
        sudo chown $RUNUSER /var/log/$PROG_NAME
        sudo cp $0  /etc/init.d
	sudo update-rc.d $SCRIPT_NAME defaults
        ;;
    reinstall)
        if [ ! -f $BIN_PATH/$PROG_NAME ]; then
            echo "$BIN_PATH/$PROG_NAME does not exists"
            exit 1
        fi
        sudo mkdir /var/log/$PROG_NAME 2>/dev/null
        sudo chown $RUNUSER /var/log/$PROG_NAME
        sudo cp $0  /etc/init.d
	sudo update-rc.d $SCRIPT_NAME defaults
        #sudo systemctl daemon-reload
        ;;
    *)
        echo "Usage: $0 {start|stop|status}"
        echo "Install/Remove Autostart: $0 install|reinstall|remove"
        echo "After install: systemctl start|stop|status $PROG_NAME"
        echo "              (systemctl disable|enable does not work)"
        exit 1
        ;;
esac
 
exit 0

