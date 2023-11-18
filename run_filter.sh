#!/bin/bash
exec -a "$1" ./a.out "${@:2}"
