#!/usr/bin/env bash
# Generates synthetic test videos in data/ for checking rppg_detect.
# None of them contain a face, so the ones that pass the length check should
# end with "no face found" (exit code 3). Each clip targets one check:
#
#   test_h264.mp4        12 s, H.264, constant 30 fps
#   test_raw.avi         12 s, uncompressed AVI (like UBFC-rPPG)
#   test_short.mp4        5 s, too short (exit code 2)
#   test_compressed.mp4  12 s, very low bitrate (heavy compression warning)
#   test_vfr.mp4         12 s, uneven frame timing (variable frame rate note)
#
# Usage: scripts/make_test_clips.sh   (run from the repo root)

set -euo pipefail

out_dir="data"
mkdir -p "$out_dir"

# testsrc draws a color test pattern; lavfi lets ffmpeg use it as an input.
# The noise filter adds random grain to every frame, like a real camera
# sensor. Without it the flat pattern compresses so well that every clip
# looks "heavily compressed" in bits per pixel.
pattern() {
    local seconds="$1" size="$2"
    echo "testsrc=duration=${seconds}:size=${size}:rate=30,noise=alls=20:allf=t+u"
}

ffmpeg -loglevel error -y -f lavfi -i "$(pattern 12 640x480)" \
    -c:v libx264 -crf 18 -pix_fmt yuv420p "$out_dir/test_h264.mp4"

# 320x240 keeps the uncompressed file around 80 MB.
ffmpeg -loglevel error -y -f lavfi -i "$(pattern 12 320x240)" \
    -c:v rawvideo -pix_fmt bgr24 "$out_dir/test_raw.avi"

ffmpeg -loglevel error -y -f lavfi -i "$(pattern 5 640x480)" \
    -c:v libx264 -crf 18 -pix_fmt yuv420p "$out_dir/test_short.mp4"

ffmpeg -loglevel error -y -f lavfi -i "$(pattern 12 640x480)" \
    -c:v libx264 -b:v 40k -pix_fmt yuv420p "$out_dir/test_compressed.mp4"

# setpts rewrites each frame's timestamp: frame N is shown at N/30 s plus a
# wobble of up to +-8 ms, so the gaps between frames are uneven.
# settb first switches to a fine clock (1/90000 s); on the default 1/30 s clock
# the wobble would get rounded away to whole frames.
# -fps_mode passthrough keeps those timestamps instead of forcing 30 fps.
ffmpeg -loglevel error -y -f lavfi -i "$(pattern 12 640x480)" \
    -vf "settb=1/90000,setpts='(N/30 + 0.008*sin(N))/TB'" -fps_mode passthrough \
    -enc_time_base 1/90000 -video_track_timescale 90000 \
    -c:v libx264 -crf 18 -pix_fmt yuv420p "$out_dir/test_vfr.mp4"

echo "Wrote test clips to $out_dir/:"
ls -lh "$out_dir"/test_*
