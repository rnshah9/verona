FROM --platform=linux/amd64 ubuntu:20.04 as builder

RUN apt-get update
RUN DEBIAN_FRONTEND=noninteractive apt-get install -y cmake ninja-build python3 \
    clang-8 clang-format clang-tools build-essential

COPY . /repo
WORKDIR /repo/build
RUN DEBIAN_FRONTEND=noninteractive apt-get install -y git
RUN cmake .. -GNinja -DVERONA_DOWNLOAD_LLVM=ON
RUN ninja

FROM ubuntu:20.04 as package
COPY --from=builder /repo/build/dist/verona-parser /
