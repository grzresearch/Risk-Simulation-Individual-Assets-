# Multi-stage build for macro_risk
# ---- builder ---------------------------------------------------------------
FROM ubuntu:24.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential \
        cmake \
        git \
        ca-certificates \
        libcurl4-openssl-dev \
        libpq-dev \
        nlohmann-json3-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY CMakeLists.txt ./
COPY src ./src
COPY tests ./tests

RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=ON \
    && cmake --build build -j \
    && ctest --test-dir build --output-on-failure

# ---- runtime ---------------------------------------------------------------
FROM ubuntu:24.04 AS runtime

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
        ca-certificates \
        libcurl4 \
        libpq5 \
    && rm -rf /var/lib/apt/lists/* \
    && useradd -m -u 10001 -s /usr/sbin/nologin macro

COPY --from=builder /src/build/macro_risk /usr/local/bin/macro_risk

USER macro
WORKDIR /home/macro
RUN mkdir -p .cache .cache/runs .cache/consensus

EXPOSE 8080
ENTRYPOINT ["/usr/local/bin/macro_risk"]
CMD ["--help"]
