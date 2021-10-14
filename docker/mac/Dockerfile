FROM ubuntu:20.04

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update
RUN apt-get install -qq cmake curl git libzstd1 libzstd-dev python3-pytest sudo zlib1g zlib1g-dev libssl-dev clang llvm

# Grant sudo access to pyth user
RUN echo "pyth ALL=(ALL) NOPASSWD:ALL" >> /etc/sudoers

RUN useradd -m pyth
USER pyth
WORKDIR /home/pyth

COPY --chown=pyth:pyth . pyth-client/

ENV CC=clang
ENV CXX=clang++
RUN cd pyth-client && mkdir build && cd build && cmake .. && make

RUN echo "\nexport PATH=\$PATH:\$HOME/pyth-client/build" >> .profile
RUN echo "\nexport PYTHONPATH=\${PYTHONPATH:+\${PYTHONPATH}:}\$HOME/pyth-client" >> .profile
