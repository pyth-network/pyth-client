ARG SOLANA_VERSION

FROM solanalabs/solana:v${SOLANA_VERSION}

RUN apt-get update
RUN apt-get install -y cmake curl g++ git libzstd1 libzstd-dev sudo zlib1g zlib1g-dev

# Grant sudo access to pyth user
RUN echo "pyth ALL=(ALL) NOPASSWD:ALL" >> /etc/sudoers

RUN useradd -m pyth
USER pyth
WORKDIR /home/pyth

COPY --chown=pyth:pyth ${GITHUB_WORKSPACE} pyth-client/

RUN cd pyth-client && mkdir build && cd build && cmake .. && make && ctest

RUN echo "\nexport PATH=\$PATH:$HOME/pyth-client/build" >> .bashrc

# Install Rust
RUN curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh -s -- -y --default-toolchain none

# Link Solana location for makefile
RUN mkdir solana && cp -a /usr/bin/sdk solana

# Build the program
RUN PATH=$PATH:$HOME/.cargo/bin && cd pyth-client/program && make

# Print hash of the program
RUN sha256sum -b pyth-client/target/oracle.so

ENTRYPOINT []
CMD []

