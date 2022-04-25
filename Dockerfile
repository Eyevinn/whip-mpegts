# syntax=docker/dockerfile:1
FROM ubuntu:focal
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update
RUN apt-get -y install libgstreamer1.0-0 gstreamer1.0-plugins-bad gstreamer1.0-plugins-good gstreamer1.0-libav gstreamer1.0-plugins-rtp gstreamer1.0-nice libsoup2.4-1 cmake gcc g++ make gdb libglib2.0-dev libgstreamer1.0-dev libgstreamer-plugins-bad1.0-dev libsoup2.4-dev

WORKDIR /src
ADD ./ /src

RUN cmake -DCMAKE_BUILD_TYPE=Release -G "Unix Makefiles" .
RUN make

FROM ubuntu:focal
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update
RUN apt-get -y install libgstreamer1.0-0 gstreamer1.0-plugins-bad gstreamer1.0-plugins-good gstreamer1.0-libav gstreamer1.0-plugins-rtp gstreamer1.0-nice libsoup2.4-1 gstreamer1.0-tools

WORKDIR /app
COPY --from=0 /src/mpeg-ts-client ./mpeg-ts-client

ENV GST_PLUGIN_PATH=/usr/lib/x86_64-linux-gnu/gstreamer-1.0/
ENV GST_PLUGIN_SCANNER=/usr/lib/x86_64-linux-gnu/gstreamer1.0/gstreamer-1.0/gst-plugin-scanner

ENTRYPOINT ["./mpeg-ts-client"]

