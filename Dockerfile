# Use Ubuntu 22.04 as base image
FROM ubuntu:22.04

# Set environment variables
ENV DEBIAN_FRONTEND=noninteractive

# Install dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    libtool \
    autotools-dev \
    automake \
    pkg-config \
    libssl-dev \
    libevent-dev \
    bsdmainutils \
    python3 \
    libboost-system-dev \
    libboost-filesystem-dev \
    libboost-chrono-dev \
    libboost-program-options-dev \
    libboost-test-dev \
    libboost-thread-dev \
    libminiupnpc-dev \
    libzmq3-dev \
    git \
    curl \
    && rm -rf /var/lib/apt/lists/*

# Set working directory
WORKDIR /bsv_fork

# Copy source code
COPY . .

# Build the project with optimized settings
RUN ./autogen.sh && \
    ./configure --disable-wallet --disable-tests --disable-bench --without-gui && \
    make -j1

# Create data directory
RUN mkdir -p /root/.bitcoin

# Copy default config
COPY bitcoin.conf /root/.bitcoin/bitcoin.conf

# Expose ports
EXPOSE 8333 8332

# Set volume for blockchain data
VOLUME ["/root/.bitcoin"]

# Start the node
CMD ["./src/bitcoind", "-printtoconsole"]
