#!/bin/bash
echo "My args: $@"
exec -a $1 ./a.out "${@:2}"
