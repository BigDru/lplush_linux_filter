#!/bin/bash
while true; do clear; ./build.sh; cat args | xargs sudo ./run_filter.sh; sleep 5; done
