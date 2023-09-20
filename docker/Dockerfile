ARG SOLANA_VERSION=v1.14.7
ARG SOLANA_DOCKER_IMAGE_HASH=9130b4a26ec00c4ba99c023da98c06abb0dba2904c990af7770928e0f7dfd2d5

FROM solanalabs/solana:v${SOLANA_VERSION}@sha256:${SOLANA_DOCKER_IMAGE_HASH}

# Redeclare SOLANA_VERSION in the new build stage.
# Persist in env for docker run & inspect.
ENV SOLANA_VERSION="${SOLANA_VERSION}"

RUN apt-get update
RUN apt-get install -qq \
  cmake \
  curl \
  g++ \
  gcc-multilib \
  git \
  libzstd1 \
  libzstd-dev \
  sudo \
  zlib1g \
  zlib1g-dev \
  qtbase5-dev \
  qtchooser \
  qt5-qmake \
  qtbase5-dev-tools \
  libqt5websockets5-dev\
  libclang-dev

# Install jcon-cpp library
RUN git clone https://github.com/joncol/jcon-cpp.git /jcon-cpp && cd /jcon-cpp && git checkout 2235654e39c7af505d7158bf996e47e37a23d6e3 && mkdir build && cd build && cmake .. && make -j4 && make install

# Grant sudo access to pyth user
RUN echo "pyth ALL=(ALL) NOPASSWD:ALL" >> /etc/sudoers
RUN useradd -m pyth

# Fixes a bug in the solana docker image
# https://github.com/solana-labs/solana/issues/20577
RUN mkdir /usr/bin/sdk/bpf/dependencies \
    && chmod -R 777 /usr/bin/sdk/bpf


USER pyth
WORKDIR /home/pyth

#Install rust
RUN curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs \
  | sh -s -- -y 

ENV PATH=${PATH}:/home/pyth/.cargo/bin

COPY --chown=pyth:pyth ./scripts/get-bpf-tools.sh pyth-client/scripts/
RUN ./pyth-client/scripts/get-bpf-tools.sh

# Apply patch to solana
COPY --chown=pyth:pyth ./scripts/patch-solana.sh ./scripts/solana.patch pyth-client/scripts/
RUN ./pyth-client/scripts/patch-solana.sh

# Copy C/C++ sources
COPY --chown=pyth:pyth ./pc pyth-client/pc
COPY --chown=pyth:pyth ./pctest pyth-client/pctest
COPY --chown=pyth:pyth ./pcapps pyth-client/pcapps
COPY --chown=pyth:pyth ./program/c pyth-client/program/c
COPY --chown=pyth:pyth ./CMakeLists.txt pyth-client
COPY --chown=pyth:pyth ./scripts/build.sh pyth-client/scripts/

# Build C/C++ offchain binaries.
RUN cd pyth-client && ./scripts/build.sh

# layer-cache crates.io packages from Cargo.{toml|lock} and rustup toolchain
COPY --chown=pyth:pyth program/rust/Cargo.toml pyth-client/program/rust/
COPY --chown=pyth:pyth Cargo.toml Cargo.lock rust-toolchain pyth-client/
RUN mkdir -p pyth-client/program/rust/src && touch pyth-client/program/rust/src/lib.rs
RUN cd pyth-client && cargo fetch --locked

# Do final source code copy to overwrite the placeholder lib.rs
COPY --chown=pyth:pyth ./ pyth-client/

# Build and test the oracle program.
RUN cd pyth-client && ./scripts/build-bpf.sh .

ENTRYPOINT []
CMD []
