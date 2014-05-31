#!/bin/sh

# start data-router
if (/bin/ps -e | /bin/grep data-router); then
  echo "Already DR activated. No need launch DR"
else
  echo "Launch DR"
  /usr/bin/data-router &
fi
