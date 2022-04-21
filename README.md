# MPEG-TS client for the WebRTC-HTTP ingestion protocol

MPEG-TS ingestion client for WHIP (https://github.com/Eyevinn/whip). Ingests an MPEG-TS unicast or multicast stream and sends it to a WHIP endpoint.

## Getting started

Supported platforms are Ubuntu 21.10 and OSX.

### Ubuntu 21.10

Install dependencies:

```
apt-get install libgstreamer1.0-0 gstreamer1.0-plugins-bad gstreamer1.0-plugins-good gstreamer1.0-libav gstreamer1.0-plugins-rtp gstreamer1.0-nice libsoup2.4-1 cmake gcc g++ make gdb libglib2.0-dev libgstreamer1.0-dev libgstreamer-plugins-bad1.0-dev libsoup2.4-dev
```

Build:

```
cmake -DCMAKE_BUILD_TYPE=Release -G "Unix Makefiles" .
make
```

Run:
```
./mpeg-ts-client <MPEG-TS address> <MPEG-TS port> <WHIP endpoint URL> [WHIP auth key]
```

### OSX

Requirements:

XCode command line tools installed.

Install additional dependencies using homebrew:
```
brew install gstreamer gst-plugins-good gst-plugins-bad libsoup@2 cmake gst-libav
```

Build:

```
cmake -DCMAKE_BUILD_TYPE=Release -G "Unix Makefiles" .
make
```

Run:
```
export GST_PLUGIN_PATH=/usr/local/lib/gstreamer-1.0/
./mpeg-ts-client <MPEG-TS address> <MPEG-TS port> <WHIP endpoint URL> [WHIP auth key]
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

## About Eyevinn Technology

Eyevinn Technology is an independent consultant firm specialized in video and streaming. Independent in a way that we are not commercially tied to any platform or technology vendor.

At Eyevinn, every software developer consultant has a dedicated budget reserved for open source development and contribution to the open source community. This give us room for innovation, team building and personal competence development. And also gives us as a company a way to contribute back to the open source community.

Want to know more about Eyevinn and how it is to work here. Contact us at work@eyevinn.se!
