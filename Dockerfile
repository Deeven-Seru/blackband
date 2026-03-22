FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

# Install build dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    ninja-build \
    wget \
    curl \
    libcurl4-openssl-dev \
    lsb-release \
    software-properties-common \
    gnupg \
    && rm -rf /var/lib/apt/lists/*

# Install LLVM 17
RUN wget -qO- https://apt.llvm.org/llvm.sh | bash -s -- 17 \
    && apt-get install -y llvm-17 llvm-17-dev clang-17 \
    && ln -sf /usr/bin/llvm-config-17 /usr/bin/llvm-config \
    && rm -rf /var/lib/apt/lists/*

# Set working directory
WORKDIR /agentc

# Copy the full repo
COPY . .

# Build the AgentC compiler
RUN echo "[BUILD] Configuring CMake..." \
    && mkdir -p agentc/build \
    && cd agentc/build \
    && cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Release \
    && echo "[BUILD] Building compiler..." \
    && ninja -j$(nproc) \
    && echo "[BUILD] Compiler built successfully."

# Run the test suite on container start
WORKDIR /agentc
CMD ["/bin/bash", "/agentc/test.sh"]
