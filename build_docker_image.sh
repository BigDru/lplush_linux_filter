#!/bin/bash
docker buildx build --platform linux/arm64/v8 -t dru_filter_image . --load
