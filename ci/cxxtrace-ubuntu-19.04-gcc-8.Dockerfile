FROM ubuntu:19.04
RUN apt-get update && apt-get install --yes \
  binutils \
  cmake \
  g++-8 \
  gcc-8 \
  libc-dev \
  libstdc++-8-dev \
  make
