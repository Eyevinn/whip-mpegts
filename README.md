# MPEG-TS client for the WebRTC-HTTP ingestion protocol

MPEG-TS ingestion client for WHIP (https://github.com/Eyevinn/whip). Ingests an MPEG-TS unicast or multicast or [SRT](https://srtalliance.org/) stream and sends it to a WHIP endpoint.

## Getting started

Supported platforms are Ubuntu 20.04, 21.10 and OSX.


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
  -t, --showTimer
  -s, --srtTransport
  --tsDemuxLatency INT
  --jitterBufferLatency INT
  --srtSourceLatency INT
  --no-audio
  --no-video
```

Flags:

- \-t Enable burned in timer
- \-s Setup SRT socket in listener mode for receiving MPEG-TS and also use SRT when restreaming

### Quick Start
To play out a testing stream and watch it in browser, we can use [Broadcast Box](https://github.com/Glimesh/broadcast-box).

```
// Generate a testing stream with GStreamer
gst-launch-1.0 -v \
    videotestsrc ! clockoverlay ! video/x-raw, height=360, width=640 ! videoconvert ! x264enc tune=zerolatency ! video/x-h264, profile=constrained-baseline ! mux. \
    audiotestsrc ! audio/x-raw, format=S16LE, channels=2, rate=44100 ! audioconvert ! voaacenc ! aacparse ! mux. \
    mpegtsmux name=mux ! queue ! srtsink uri="srt://127.0.0.1:9998?mode=caller" wait-for-connection=false

// Start whip-mpegts with you own Stream Key (e.g., testingstream123) and use Broadcast Box as WHIP endpoint
./whip-mpegts -a "127.0.0.1" -p 9998 -u "https://b.siobud.com/api/whip" -k "testingstream123"  -s
```
Open [Broadcast Box](https://b.siobud.com) in browser and type in the same Stream Key (e.g., testingstream123) and click "Watch Stream".

### Build Ubuntu 21.10

Install dependencies:

```
apt-get install libgstreamer1.0-0 gstreamer1.0-plugins-bad gstreamer1.0-plugins-good gstreamer1.0-libav gstreamer1.0-plugins-rtp gstreamer1.0-nice libsoup2.4-1 cmake gcc g++ make gdb libglib2.0-dev libgstreamer1.0-dev libgstreamer-plugins-bad1.0-dev libsoup2.4-dev
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
brew install gstreamer gst-plugins-good gst-plugins-bad libsoup@2 cmake gst-libav
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
