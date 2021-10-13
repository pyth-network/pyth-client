FROM ubuntu:20.04

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update
RUN apt-get install -qq cmake curl g++ git libzstd1 libzstd-dev python3-pytest sudo zlib1g zlib1g-dev libssl-dev

# Grant sudo access to pyth user
RUN echo "pyth ALL=(ALL) NOPASSWD:ALL" >> /etc/sudoers

RUN useradd -m pyth
USER pyth
WORKDIR /home/pyth

# Install AFL
RUN git clone https://github.com/google/AFL.git --branch v2.57b afl
RUN cd afl && make && cd /home/pyth
# Build everything with the AFL compilers
ENV CC=/home/pyth/afl/afl-gcc
ENV CXX=/home/pyth/afl/afl-g++

COPY --chown=pyth:pyth . pyth-client/
RUN cd pyth-client && mkdir build && cd build && cmake .. && make
