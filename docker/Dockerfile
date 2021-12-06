ARG SOLANA_VERSION
FROM solanalabs/solana:v${SOLANA_VERSION}

# Redeclare SOLANA_VERSION in the new build stage.
# Persist in env for docker run & inspect.
ARG SOLANA_VERSION
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
  python3-pytest \
  python3-websockets \
  sudo \
  zlib1g \
  zlib1g-dev

# Grant sudo access to pyth user
RUN echo "pyth ALL=(ALL) NOPASSWD:ALL" >> /etc/sudoers
RUN useradd -m pyth

USER pyth
WORKDIR /home/pyth
COPY --chown=pyth:pyth . pyth-client/

RUN echo "\n\
export PATH=\"\${PATH}:\${HOME}/pyth-client/build\"\n\
export PYTHONPATH=\"\${PYTHONPATH:+\$PYTHONPATH:}\${HOME}/pyth-client\"\n\
" >> .profile

# Build off-chain binaries.
RUN cd pyth-client && ./scripts/build.sh

# Install rust and add ". ~/.cargo/env" to ~/.profile.
RUN curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs \
  | sh -s -- -y --default-toolchain none

# Copy solana sdk and apply patch.
RUN mkdir solana
RUN cp -a /usr/bin/sdk solana
RUN ./pyth-client/scripts/patch-solana.sh

# Build and test the oracle program.
RUN cd pyth-client && ./scripts/build-bpf.sh program
RUN /bin/bash -l -c "pytest-3 --pyargs pyth"

ENTRYPOINT []
CMD []
