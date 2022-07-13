FROM ubuntu:20.04@sha256:fd92c36d3cb9b1d027c4d2a72c6bf0125da82425fc2ca37c414d4f010180dc19

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update
RUN apt-get install -qq cmake curl git libzstd1 libzstd-dev python3-pytest sudo zlib1g zlib1g-dev libssl-dev clang llvm

# Grant sudo access to pyth user
RUN echo "pyth ALL=(ALL) NOPASSWD:ALL" >> /etc/sudoers

RUN useradd -m pyth
USER pyth
WORKDIR /home/pyth

# Install AFL
ENV CC=clang
ENV CXX=clang++
RUN git clone https://github.com/google/AFL.git --branch v2.57b afl
RUN cd afl && make && cd llvm_mode && make && cd /home/pyth

# Build everything with the AFL compilers
ENV CC=/home/pyth/afl/afl-clang-fast
ENV CXX=/home/pyth/afl/afl-clang-fast++

COPY --chown=pyth:pyth . pyth-client/
RUN cd pyth-client && mkdir build && cd build && cmake .. && make
