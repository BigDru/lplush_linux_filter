#!/bin/bash
args_array=()
while IFS= read -r -d '"' open; do
    read -r -d '"' arg
    #echo \"$arg\"
    args_array+=("$arg")
done < args.txt

#echo "${args_array[*]}"

# Use exec to replace the shell with gdb, preserving the argument list
gdb -x commands.gdb --args ./a.out "${args_array[@]}"
