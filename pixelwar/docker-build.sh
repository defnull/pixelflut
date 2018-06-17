#!/bin/bash

# Building container image from Dockerfile
docker build -t pixelwar .

# Start container, copy artifacts and stop again
docker run -d --rm --name pixelwar1 pixelwar sleep 99999
docker cp pixelwar1:/pixelwar/target .
docker stop pixelwar1

echo "Enter to remove container image or CTRL-C to abort."
read
docker rmi pixelwar
