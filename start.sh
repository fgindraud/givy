#!/usr/bin/bash

name_server_uri=$(./name_server)

echo "Name server started at $name_server_uri"

export GIVY_NAME_SERVER="$name_server_uri"
export GIVY_NB_NODE="${1:-4}"
for ((i = 0; i < $GIVY_NB_NODE; i++)); do
	export GIVY_NODE_ID="$i"
	./givy &
done
wait
