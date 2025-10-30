# syntax=docker/dockerfile:1
FROM debian:trixie
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update
RUN apt-get -y install libgstreamer1.0-0 gstreamer1.0-plugins-bad gstreamer1.0-plugins-good gstreamer1.0-libav gstreamer1.0-plugins-rtp gstreamer1.0-plugins-ugly gstreamer1.0-nice libsoup-3.0-0 cmake gcc g++ make gdb libglib2.0-dev libgstreamer1.0-dev libgstreamer-plugins-bad1.0-dev libsoup-3.0-dev pkg-config

WORKDIR /src
ADD ./ /src

RUN cmake -DCMAKE_BUILD_TYPE=Release -G "Unix Makefiles" .
RUN make

FROM debian:trixie
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update
RUN apt-get -y install libgstreamer1.0-0 gstreamer1.0-plugins-bad gstreamer1.0-plugins-good gstreamer1.0-libav gstreamer1.0-plugins-rtp gstreamer1.0-plugins-ugly gstreamer1.0-nice libsoup-3.0-0 gstreamer1.0-tools

WORKDIR /app
COPY --from=0 /src/whip-mpegts ./whip-mpegts

ENTRYPOINT ["./whip-mpegts"]
