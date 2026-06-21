#!/bin/sh

# Host directory that will hold the downloaded playback videos.
DOWNLOADS="$HOME/Videos/playbacks"

# Create the directory if it is missing. The container runs as root (see
# Dockerfile), so it can write here regardless of ownership; the downloaded
# files will be owned by root on the host (use sudo to delete/move them).
if [ ! -d "$DOWNLOADS" ]; then
    echo "Directory $DOWNLOADS does not exist. Creating it..."
    mkdir -p "$DOWNLOADS"
fi

docker run --rm -it \
    -v "$DOWNLOADS":/var/lib/visionplayback/downloads \
    -p 18068:18068 \
    visionplayback:latest --api-key=my-secret-key
