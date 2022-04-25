#!/bin/bash

docker run -ti --rm -v ${PWD}:/app -w /app mpeg-ts-client-builder:latest docker/builder/build.sh
docker build -t mpeg-ts-client:latest .
