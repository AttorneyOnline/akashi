###
### Build
###
FROM ubuntu:noble@sha256:9cbed754112939e914291337b5e554b07ad7c392491dba6daf25eef1332a22e8

# Install build dependencies
RUN apt-get update && apt-get install -y build-essential cmake qt6-base-dev qt6-websockets-dev

# Copy files into image
RUN mkdir /build
COPY . /build

# Build akashi
WORKDIR /build
RUN cmake -B build -D CMAKE_BUILD_TYPE=Release && cmake --build build -j$(nproc)

###
### Run
###
FROM ubuntu:noble@sha256:9cbed754112939e914291337b5e554b07ad7c392491dba6daf25eef1332a22e8

# Install runtime dependencies
RUN apt-get update && apt-get install -y qt6-base-dev qt6-websockets-dev libqt6sql6-sqlite && rm -rf /var/lib/apt/lists/*

# Copy built assets
RUN mkdir /app
COPY --from=0 /build/bin /app
COPY ./docker-entrypoint.sh /app

# Run akashi
WORKDIR /app
ENTRYPOINT [ "bash" ]
CMD ["-c", "./docker-entrypoint.sh"]
