#!/bin/sh
cat $1|sed '$d'|sed 's/^:........//'|sed 's/...$//'|sed 's/\(..\)/0x\1,/g'
