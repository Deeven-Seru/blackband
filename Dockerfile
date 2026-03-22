# AgentC — Reproducible Build & Test Container
#
# Build:  docker build -t agentc .
# Run:    docker run --rm agentc
# Shell:  docker run --rm -it agentc /bin/bash

FROM ubuntu:22.04

# ─── Environment ─────────────────────────────────────────────────────────────
ENV DEBIAN_FRONTEND=noninteractive \
    LLVM_VERSION=17 \
    CMAKE_BUILD_TYPE=Release

# ─── System dependencies ─────────────────────────────────────────────────────
RUN apt-get update -qq && \
    apt-get install -y --no-install-recommends \
        ca-certificates \
        wget \
        gnupg \
        lsb-release \
        software-properties-common && \
    # LLVM official apt repository
    wget -qO- https://apt.llvm.org/llvm-snapshot.gpg.key | \
        gpg --dearmor -o /usr/share/keyrings/llvm-archive-keyring.gpg && \
    echo "deb [signed-by=/usr/share/keyrings/llvm-archive-keyring.gpg] \
        https://apt.llvm.org/jammy/ llvm-toolchain-jammy-17 main" \
        > /etc/apt/sources.list.d/llvm-17.list && \
    apt-get update -qq && \
    apt-get install -y --no-install-recommends \
        cmake \
        ninja-build \
        g++ \
        libcurl4-openssl-dev \
        llvm-17 \
        llvm-17-dev \
        clang-17 \
        libclang-17-dev \
        lld-17 \
        pkg-config \
        git \
        jq && \
    # Make llvm-17 tools the default
    update-alternatives --install /usr/bin/llvm-config llvm-config \
        /usr/bin/llvm-config-17 100 && \
    update-alternatives --install /usr/bin/clang++    clang++ \
        /usr/bin/clang++-17    100 && \
    # Cleanup
    rm -rf /var/lib/apt/lists/*

# ─── Copy source ─────────────────────────────────────────────────────────────
WORKDIR /workspace
COPY . .

# ─── Build compiler ──────────────────────────────────────────────────────────
RUN cmake -S agentc -B agentc/build \
        -G Ninja \
        -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} \
        -DCMAKE_CXX_COMPILER=clang++-17 \
    && cmake --build agentc/build --parallel $(nproc)

# ─── Run tests & generate report ─────────────────────────────────────────────
RUN chmod +x verify_tests.sh && \
    ./verify_tests.sh 2>&1 | tee /tmp/results/build.log ; \
    exit ${PIPESTATUS[0]}

# ─── Default command: display results ────────────────────────────────────────
CMD ["cat", "/tmp/results/test_results.json"]
