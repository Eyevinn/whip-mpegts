# MPEG-TS client for the WebRTC-HTTP ingestion protocol

MPEG-TS ingestion client for WHIP (https://github.com/Eyevinn/whip). Ingests an MPEG-TS unicast or multicast or [SRT](https://srtalliance.org/) stream and sends it to a WHIP endpoint.

## Getting started

Supported platforms are Ubuntu 22.04+ and OSX.


### Install binary

Homebrew:

```
brew install eyevinn/tools/whip-mpegts
```

### Usage

```
Usage: whip-mpegts [OPTION]
  -a, --udpSourceAddress STRING
  -p, --udpSourcePort INT
  -u, --whipEndpointUrl STRING
  -k, --whipEndpointAuthKey STRING
  -d, --udpSourceQueueMinTime INT ms
  -r, --restreamAddress STRING
  -o, --restreamPort INT
  -b, --h264EncodeBitrate INT kb (video encode bitrate, applies to H264 and VP8)
  -t, --showTimer
  -s, --srtTransport
  -m, --srtMode INT (1=caller, 2=listener, default=2)
  --tsDemuxLatency INT
  --jitterBufferLatency INT
  --srtSourceLatency INT
  --no-audio
  --no-video
  --bypass-audio
  --bypass-video
```

Flags:

- \-t Enable burned in timer
- \-s Enable SRT transport for receiving MPEG-TS and also use SRT when restreaming
- \-m Set SRT mode: 1 for caller (connect to remote), 2 for listener (wait for connection, default)
- \--bypass-video Skip video transcoding (no decode/encode). Only works when the input video is already H264. Cannot be combined with `--vp8`.
- \--bypass-audio Skip audio transcoding (no decode/encode). Only works when the input audio is already OPUS.

### Recommended input codecs

By default whip-mpegts **transcodes** both video and audio: it decodes the incoming stream, then re-encodes video to H264 (or VP8 with `--vp8`) and audio to OPUS before sending to the WHIP endpoint. Transcoding is the most CPU-intensive part of the pipeline.

Passthrough (no decode, no encode) is available only for the codecs WebRTC uses natively:

- **H264 video** with `--bypass-video` — the H264 stream is parsed and payloaded directly, skipping decode/encode. Requires the input video to already be H264, and cannot be combined with `--vp8`.
- **OPUS audio** with `--bypass-audio` — the OPUS stream is parsed and payloaded directly, skipping decode/encode. Requires the input audio to already be OPUS.

**H264 video + OPUS audio is therefore the least-work input combination**: with both `--bypass-video` and `--bypass-audio` the pipeline does no transcoding at all. Other codecs are always decoded and re-encoded — for example AAC audio is always decoded and re-encoded to OPUS, as there is no AAC passthrough path.

### Output bitrate

The output video bitrate is directly controllable via `-b, --h264EncodeBitrate INT`, which sets
the encoder's target bitrate in kilobits per second (kb). It defaults to **2000 kb**. Despite the
`h264` in the flag name, the value applies to both the H264 (`x264enc`) and VP8 (`vp8enc`)
encoders — it is passed as the `bitrate` property on `x264enc`, and as `target-bitrate`
(converted to bits/s) on `vp8enc` when running with `--vp8`.

Because the encoder always re-encodes to this target, the output bitrate is set by `-b` regardless
of the input stream's bitrate. The input bitrate only influences the output when transcoding is
active, and even then the encoder re-encodes to the configured target — so observing "lower input
gives lower output" is incidental, not the intended control knob. Use `-b` to control output
bitrate.

Note: this bitrate control applies to video only. There is currently no separate output audio
bitrate flag; audio is re-encoded to Opus (`opusenc`) using the encoder's default settings. When
video transcoding is skipped with `--bypass-video`, `-b` has no effect since the incoming H264 is
passed through unchanged.

### Quick Start
To play out a testing stream and watch it in browser, we can use [Broadcast Box](https://github.com/Glimesh/broadcast-box).

#### Example 1: SRT Listener Mode (default)
```bash
# Generate a testing stream with GStreamer as SRT caller
gst-launch-1.0 -v \
    videotestsrc ! clockoverlay ! video/x-raw, height=360, width=640 ! videoconvert ! x264enc tune=zerolatency ! video/x-h264, profile=constrained-baseline ! mux. \
    audiotestsrc ! audio/x-raw, format=S16LE, channels=2, rate=44100 ! audioconvert ! voaacenc ! aacparse ! mux. \
    mpegtsmux name=mux ! queue ! srtsink uri="srt://127.0.0.1:9998?mode=caller" wait-for-connection=false

# Start whip-mpegts in listener mode (waits for incoming connection)
./whip-mpegts -a "127.0.0.1" -p 9998 -u "https://b.siobud.com/api/whip" -k "testingstream123" -s
```

#### Example 2: SRT Caller Mode
```bash
# Generate a testing stream with FFmpeg as SRT listener
ffmpeg -re -f lavfi -i testsrc=size=1280x720:rate=30 -f lavfi -i sine=frequency=1000:sample_rate=48000 \
    -c:v libx264 -preset ultrafast -tune zerolatency -b:v 2000k -c:a aac -b:a 128k \
    -f mpegts "srt://127.0.0.1:9998?mode=listener"

# Start whip-mpegts in caller mode (connects to remote endpoint)
./whip-mpegts -s -m 1 -a "127.0.0.1" -p 9998 -u "https://b.siobud.com/api/whip" -k "testingstream123"
```

Open [Broadcast Box](https://b.siobud.com) in browser and type in the same Stream Key (e.g., testingstream123) and click "Watch Stream".

### No-transcode passthrough (H264 / OPUS)

By default, `whip-mpegts` decodes the incoming MPEG-TS elementary streams and
re-encodes them (H264 video via `x264enc`, OPUS audio via `opusenc`) before
sending them to the WHIP endpoint. When the source is already encoded as H264
video and/or OPUS audio, you can skip that decode/encode step and forward the
elementary streams unchanged:

- `--bypass-video` — skip video transcoding. Only works with H264. The parsed
  H264 access units are payloaded (`rtph264pay`) and sent as-is; no decoder or
  encoder is inserted.
- `--bypass-audio` — skip audio transcoding. Only works with OPUS. The parsed
  OPUS frames are payloaded (`rtpopuspay`) and sent as-is; no decoder or encoder
  is inserted.

The two flags are independent — you can bypass video, audio, or both. Because
nothing on the bypass path modifies the media, the incoming stream must already
match what the WHIP endpoint accepts: **H264 video for `--bypass-video`, OPUS
audio for `--bypass-audio`**. If the demuxed video is not `video/x-h264` (or the
audio is not `audio/x-opus`), the bypass pad handler will not be linked.

On the bypass path all encoder-controlled properties are the **source encoder's
responsibility**, not this tool's:

- H264 profile/level (e.g. constrained-baseline) — set on the source encoder.
- Keyframe (IDR) cadence — set on the source encoder; the WHIP/WebRTC receiver
  relies on the source's GOP structure.
- Bitrate — `-b, --h264EncodeBitrate` has no effect on a bypassed stream, since
  no re-encode happens.

`--bypass-video` cannot be combined with `--vp8`; the tool exits with an error
if both are given (VP8 requires transcoding).

> **Note:** Configuring the WHIP/SFU endpoint itself to accept and forward H264
> is out of scope for this repository and is tracked separately.

#### Example: bypass-compatible H264/OPUS MPEG-TS

Produce a stream whose video is already H264 (constrained-baseline) and whose
audio is already OPUS, muxed into MPEG-TS over SRT, then run `whip-mpegts` with
both bypass flags:

```bash
# Generate an H264/OPUS testing stream with GStreamer as SRT caller
gst-launch-1.0 -v \
    videotestsrc ! clockoverlay ! video/x-raw, height=360, width=640 ! videoconvert ! x264enc tune=zerolatency key-int-max=30 ! video/x-h264, profile=constrained-baseline ! mux. \
    audiotestsrc ! audio/x-raw, format=S16LE, channels=2, rate=48000 ! audioconvert ! opusenc ! mux. \
    mpegtsmux name=mux ! queue ! srtsink uri="srt://127.0.0.1:9998?mode=caller" wait-for-connection=false

# Start whip-mpegts in listener mode with no transcoding on either track
./whip-mpegts -a "127.0.0.1" -p 9998 -u "https://b.siobud.com/api/whip" -k "testingstream123" -s --bypass-video --bypass-audio
```

Here `key-int-max=30` and `profile=constrained-baseline` are chosen on the
source encoder, since on the bypass path this tool forwards them unchanged.

## Debugging

### Pipeline State Debugging

You can generate GStreamer pipeline dot files for debugging by setting the `GST_DEBUG_DUMP_DOT_DIR` environment variable. When this variable is set, the application will install a SIGHUP signal handler that allows you to dump the current pipeline state on demand.

**Setup:**

```bash
# Create a directory for dot files
mkdir -p /tmp/gst-dots

# Set the environment variable
export GST_DEBUG_DUMP_DOT_DIR=/tmp/gst-dots

# Run whip-mpegts
./whip-mpegts -a "127.0.0.1" -p 9998 -u "https://b.siobud.com/api/whip" -k "testingstream123"
```

**Trigger pipeline state dump:**

While the application is running, send a SIGHUP signal to dump the current pipeline state:

```bash
# Find the process ID
ps aux | grep whip-mpegts

# Send SIGHUP signal
kill -SIGHUP <pid>
```

This will create a `pipeline-sighup.dot` file in the directory specified by `GST_DEBUG_DUMP_DOT_DIR`. You can convert the dot file to an image:

```bash
# Convert to PNG
dot -Tpng /tmp/gst-dots/pipeline-sighup.dot -o pipeline.png

# Convert to SVG
dot -Tsvg /tmp/gst-dots/pipeline-sighup.dot -o pipeline.svg
```

**Note:** If `GST_DEBUG_DUMP_DOT_DIR` is not set, the SIGHUP handler will not be installed.

### Build Ubuntu/Debian

Install dependencies:

```
apt-get install libgstreamer1.0-0 gstreamer1.0-plugins-bad gstreamer1.0-plugins-good gstreamer1.0-libav gstreamer1.0-plugins-rtp gstreamer1.0-plugins-ugly gstreamer1.0-nice libsoup-3.0-0 cmake gcc g++ make gdb libglib2.0-dev libgstreamer1.0-dev libgstreamer-plugins-bad1.0-dev libsoup-3.0-dev pkg-config
```

Build:

```
cmake -DCMAKE_BUILD_TYPE=Release -G "Unix Makefiles" .
make
```

### Build OSX

Requirements:

XCode command line tools installed.

Install additional dependencies using homebrew:
```
brew install gstreamer gst-plugins-good gst-plugins-bad libsoup cmake gst-libav
```

On Apple M1 you might need to build the gst-plugins-bad from source as the SRT plugins are not available in the binary bottle.

```
brew reinstall --build-from-source gst-plugins-bad
```

Build:

```
cmake -DCMAKE_BUILD_TYPE=Release -G "Unix Makefiles" .
make
```

Notes:
For **HTTPS** requests, GLIB (libsoup) requires some environment variables:
```
export GLIB_NETWORKING=/opt/homebrew/Cellar/glib-networking/2.78.0
export GIO_MODULE_DIR=${GLIB_NETWORKING}/lib/gio/modules/
```

### Build Docker Container

Build container (uses multi-stage builds):

```
docker build -t mpegts-whip:dev .
```

Run container (example):

```
docker run --rm -p <MPEG-TS port>:<MPEG-TS port>/udp mpegts-whip:dev -a <MPEG-TS address> -p <MPEG-TS port> -u http://<WHIP endpoint URL> -k [WHIP auth key]
```

## License (Apache-2.0)

```
Copyright 2022 Eyevinn Technology AB

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
```

## Support

Join our [community on Slack](http://slack.streamingtech.se) where you can post any questions regarding any of our open source projects. Eyevinn's consulting business can also offer you:

- Further development of this component
- Customization and integration of this component into your platform
- Support and maintenance agreement

Contact [sales@eyevinn.se](mailto:sales@eyevinn.se) if you are interested.

## About Eyevinn Technology

[Eyevinn Technology](https://www.eyevinntechnology.se) is an independent consultant firm specialized in video and streaming. Independent in a way that we are not commercially tied to any platform or technology vendor. As our way to innovate and push the industry forward we develop proof-of-concepts and tools. The things we learn and the code we write we share with the industry in [blogs](https://dev.to/video) and by open sourcing the code we have written.

Want to know more about Eyevinn and how it is to work here. Contact us at work@eyevinn.se!
