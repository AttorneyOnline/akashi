### 
### Build
###
FROM ubuntu:noble@sha256:9cbed754112939e914291337b5e554b07ad7c392491dba6daf25eef1332a22e8

# Install build dependencies
RUN apt-get update && apt-get install build-essential qtbase5-dev qt5-qmake qttools5-dev qttools5-dev-tools libqt5websockets5-dev -y

# Copy files into image
RUN mkdir /build
COPY . /build

# Build akashi
WORKDIR /build
RUN qmake project-akashi.pro && make -j$(nproc)

### 
### Run
###
FROM ubuntu:noble@sha256:9cbed754112939e914291337b5e554b07ad7c392491dba6daf25eef1332a22e8

# Install runtime dependencies
RUN apt-get update && apt-get install libqt5websockets5-dev -y

# Copy built assets
RUN mkdir /app
COPY --from=0 /build/bin /app
COPY ./docker-entrypoint.sh /app

# Run akashi
WORKDIR /app
ENTRYPOINT [ "bash" ]
CMD ["-c", "./docker-entrypoint.sh"]