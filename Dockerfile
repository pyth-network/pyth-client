ARG SOLANA_VERSION
FROM solanalabs/solana:v${SOLANA_VERSION}

# Redeclare SOLANA_VERSION in the new build stage.
# Persist in env for docker run & inspect.
ARG SOLANA_VERSION
ENV SOLANA_VERSION="${SOLANA_VERSION}"


ENTRYPOINT []
CMD []
