FROM ubuntu:20.04

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update
RUN apt-get install -qq cmake curl g++ git libzstd1 libzstd-dev python3-pytest sudo zlib1g zlib1g-dev libssl-dev

# Grant sudo access to pyth user
RUN echo "pyth ALL=(ALL) NOPASSWD:ALL" >> /etc/sudoers

RUN useradd -m pyth
USER pyth
WORKDIR /home/pyth

COPY --chown=pyth:pyth . pyth-client/

RUN rm -rf pyth-client/build
RUN cd pyth-client && mkdir build && cd build && cmake .. && make && ctest

RUN echo "\nexport PATH=\$PATH:\$HOME/pyth-client/build" >> .profile

RUN echo "\nexport PYTHONPATH=\${PYTHONPATH:+\${PYTHONPATH}:}\$HOME/pyth-client" >> .profile

RUN /bin/bash -l -c "pytest-3 --pyargs pyth"
