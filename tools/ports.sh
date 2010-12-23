#!/bin/sh

SED_EXP="s/\([^ \t]*\)[ \t]*\([0-9-]*\)\/.*/  <DEF NAME=\"\1\" TYPE=\"UInt16\">\2<\/DEF>/g"
cat /etc/services | egrep "^[^#].*udp.*" | sed -e "$SED_EXP" | egrep "NAME=\"[A-z0-9./-]+\""
