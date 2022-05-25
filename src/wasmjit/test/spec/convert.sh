#! /bin/bash

WABT_PATH=`pwd`"/../../../wabt/bin/"
BIN=$WABT_PATH"wast2json"
CURR=`pwd`

echo $BIN

for file in *.wast; do
  NAME=$(basename "${file%.*}")
  $BIN $file -o $NAME".json"
done

