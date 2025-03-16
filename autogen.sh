#!/bin/sh

echo "Running Xfce Developer Tools..."

xdt-autogen $@
if [ $? -ne 0 ]; then
  echo "xdt-autogen Failed"
  echo "Please, install xfce4-dev-tools"
  echo "or verify Errors"
fi
