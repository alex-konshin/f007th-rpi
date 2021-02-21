#!/bin/bash

# -----------------------------------------------------------------------------
# Default build configuration

use_gpio_ts=1
include_MQTT=0
include_HTTPD=1
include_DS18B20=0
executable=''

# -----------------------------------------------------------------------------
# Reading build configuration file

RUNDIR=`pwd`
BUILD_HOME=`dirname $0`
BUILD_HOME=`cd $BUILD_HOME; pwd`

if [ -r "${BUILD_HOME}/build.cfg" ]; then
  echo "Reading build configuration from file \"${BUILD_HOME}/build.cfg\""
  . ${BUILD_HOME}/build.cfg
fi

# -----------------------------------------------------------------------------
# Processing configuration

is_RPI=0
is_ARM=0
is_turbot=0
machine=`uname -m`
case $machine in
  x86_64)
    is_turbot=1
    ;;
  armv6l)
    is_ARM=1
    ;;
  armv7l)
    is_ARM=1
    ;;
  *)
    echo "ERROR: Unrecognized platform (\`uname -m\` = ${machine})."
    exit 1
    ;;
esac
if [ $is_ARM -eq 1 -a -e '/proc/device-tree/model' ]; then
  model=$(tr -d '\0' </proc/device-tree/model)
  if [ ! -z "${model}" ]; then
    if [[ $model = Raspberry* ]]; then
      is_RPI=1
      echo "Building on ${model}"
    fi
  fi
fi
if [ $is_RPI -ne 1 ]; then
  # pigpio is not available on platforms other than Raspberry Pi
  use_gpio_ts=1
  # Support of DS18B20 is not available on platforms other than Raspberry Pi
  include_DS18B20=0
fi
if [ -z "${executable}" ]; then
  if [ $use_gpio_ts -eq 1 ]; then
    executable='f007th-send'
  else
    executable='f007th-rpi_send'
  fi
fi

# -----------------------------------------------------------------------------

BUILD_DIR=${BUILD_HOME}/build
if [ -e ${BUILD_DIR} ]; then
  rm -r ${BUILD_DIR}
fi
cp -R ${BUILD_HOME}/f007th-ts-all ${BUILD_DIR}
if [ $? -ne 0 ]; then
  echo "ERROR: Failed to copy directory \"${BUILD_HOME}/f007th-ts-all\" to \"${BUILD_DIR}\"."
  exit 1
fi

# -----------------------------------------------------------------------------
echo "Build will be done in directory \"${BUILD_DIR}\""
echo ""
echo "  executable      = $executable"
echo "  use_gpio_ts     = $use_gpio_ts"
echo "  include_MQTT    = $include_MQTT"
echo "  include_HTTPD   = $include_HTTPD"
echo "  include_DS18B20 = $include_DS18B20"
echo ""
# -----------------------------------------------------------------------------

platform_options=''
options='-DNDEBUG'

if [ $include_HTTPD -eq 1 ]; then
  options=${options}' -DINCLUDE_HTTPD'
fi
if [ $include_DS18B20 -eq 1 ]; then
  options=${options}' -DINCLUDE_POLLSTER'
fi
if [ $include_MQTT -eq 1 ]; then
  options=${options}' -DINCLUDE_MQTT'
fi
if [ $use_gpio_ts -eq 1 ]; then
  options='-DUSE_GPIO_TS '${options}
fi
if [ $is_RPI -eq 1 ]; then
  options='-DRPI '${options}
elif [ $is_turbot -eq 1 ]; then
  platform_options=' -U_FORTIFY_SOURCE -fno-stack-protector'
fi

# -----------------------------------------------------------------------------
process_subdir_mk() {
  file="${BUILD_DIR}/$1"
  echo "... Procesing file \"${file}\" ..."

#  g++ -std=c++11 -DRPi               -DNDEBUG -DINCLUDE_HTTPD -DINCLUDE_POLLSTER                -O2     -Wall -c -fmessage-length=0 -pthread                                        -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
#  g++ -std=c++11 -DRPI -DUSE_GPIO_TS -DNDEBUG -DINCLUDE_HTTPD -DINCLUDE_POLLSTER -DINCLUDE_MQTT -O2     -Wall -c -fmessage-length=0 -pthread                                        -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
#  g++ -std=c++11       -DUSE_GPIO_TS -DNDEBUG -DINCLUDE_HTTPD -DINCLUDE_POLLSTER                -O2     -Wall -c -fmessage-length=0 -pthread -U_FORTIFY_SOURCE -fno-stack-protector -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
# TODO
#  g++ -std=c++0x -DRPI -DTEST_DECODING        -DINCLUDE_HTTPD -DINCLUDE_POLLSTER -DINCLUDE_MQTT -O0 -g3 -Wall -c -fmessage-length=0 -pthread                                        -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"

  compile='\tg++ -std=c++11 '${options}' -O2 -Wall -c -fmessage-length=0 -pthread'${platform_options}' -MMD -MP -MF\"$(@:%.o=%.d)\" -MT\"$(@)\" -o \"$@\" \"$<\"'

  line_filter='!/^\#.*/'
  if [ "$1" = "utils/subdir.mk" ]; then
    if [ $include_MQTT -ne 1 ]; then
      line_filter='!/^\#.*/ && !/\.?\.\/utils\/MQTT\..*/'
    fi
  elif [ "$1" = "protocols/subdir.mk" ]; then
    if [ $include_DS18B20 -ne 1 ]; then
      line_filter='!/^\#.*/ && !/\.?\.\/protocols\/DS18B20\..*/'
    fi
  fi

  awk_program=${line_filter}' { if (/g\+\+/) { print "'${compile}'"} else { print } }'

  #echo "${awk_program}"
  awk "${awk_program}" "${file}" > "${file}.new"
  if [ $? -ne 0 ]; then
    echo "ERROR: Failed to process file \"${file}\"."
    exit 1
  fi
  rm -f ${file}
  if [ $? -ne 0 ]; then
    echo "ERROR: Failed to delete file \"${file}\"."
    exit 1
  fi
  mv "${file}.new" "${file}"
  if [ $? -ne 0 ]; then
    echo "ERROR: Failed to rename file \"${file}.new\" to \"${file}\"."
    exit 1
  fi
}
process_subdir_mk "common/subdir.mk"
process_subdir_mk "utils/subdir.mk"
process_subdir_mk "protocols/subdir.mk"
process_subdir_mk "subdir.mk"

# -----------------------------------------------------------------------------
process_objects_mk() {
  file="${BUILD_DIR}/$1"
  echo "... Procesing file \"${file}\" ..."

  #  LIBS := -lrt -lcurl -lmicrohttpd -lmosquitto -lmosquittopp
  libs='LIBS := -lrt -lcurl'
  if [ $use_gpio_ts -ne 1 ]; then
    libs=${libs}' -lpigpio'
  fi
  if [ $include_HTTPD -eq 1 ]; then
    libs=${libs}' -lmicrohttpd'
  fi
  if [ $include_MQTT -eq 1 ]; then
    libs=${libs}' -lmosquitto -lmosquittopp'
  fi

  awk_program='!/^\#.*/ { if (/^LIBS \:=/) { print "'${libs}'"} else { print } }'
  #echo "${awk_program}"
  awk "${awk_program}" "${file}" > "${file}.new"
  if [ $? -ne 0 ]; then
    echo "ERROR: Failed to process file \"${file}\"."
    exit 1
  fi
  rm -f ${file}
  if [ $? -ne 0 ]; then
    echo "ERROR: Failed to delete file \"${file}\"."
    exit 1
  fi
  mv "${file}.new" "${file}"
  if [ $? -ne 0 ]; then
    echo "ERROR: Failed to rename file \"${file}.new\" to \"${file}\"."
    exit 1
  fi
}
process_objects_mk "objects.mk"

# -----------------------------------------------------------------------------
process_makefile() {
  if [ "${executable}" = 'f007th-send' ]; then
    return 0
  fi
  file="${BUILD_DIR}/$1"
  echo "... Procesing build script \"${file}\" ..."

  # all: f007th-send
  # f007th-send: $(OBJS) $(USER_OBJS)
  #   g++ -pthread -o "f007th-send" $(OBJS) $(USER_OBJS) $(LIBS)
  #   -$(RM) $(CC_DEPS)$(C++_DEPS)$(EXECUTABLES)$(OBJS)$(C_UPPER_DEPS)$(CXX_DEPS)$(C_DEPS)$(CPP_DEPS) f007th-send

  awk_program='!/^\#.*/ { if (/f007th\-send/) { sub(/f007th\-send/,"'${executable}'") }; print }'
  #echo "${awk_program}"
  awk "${awk_program}" "${file}" > "${file}.new"
  if [ $? -ne 0 ]; then
    echo "ERROR: Failed to process file \"${file}\"."
    exit 1
  fi
  rm -f ${file}
  if [ $? -ne 0 ]; then
    echo "ERROR: Failed to delete file \"${file}\"."
    exit 1
  fi
  mv "${file}.new" "${file}"
  if [ $? -ne 0 ]; then
    echo "ERROR: Failed to rename file \"${file}.new\" to \"${file}\"."
    exit 1
  fi
}
process_makefile "makefile"

# -----------------------------------------------------------------------------
# Building
echo ""
echo "... Building ..."
echo ""

pushd ${BUILD_DIR} > /dev/null
make ${executable}
rc=$?
popd > /dev/null
if [ $rc -ne 0 ]; then
  echo "ERROR: Failed to make the executable."
  exit 1
fi
echo "Executale \"${BUILD_DIR}/${executable}\" has been built successfully."

# -----------------------------------------------------------------------------
echo ""
cp "${BUILD_DIR}/${executable}" "${BUILD_HOME}/bin/"
if [ $? -ne 0 ]; then
  echo "ERROR: Failed to copy file \"${BUILD_DIR}/${executable}\" to directory \"${BUILD_HOME}/bin/\"."
  exit 1
fi
echo "File \"${BUILD_DIR}/${executable}\" has been copied to directory \"${BUILD_HOME}/bin/\"."

echo ""
echo "Done."
