FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        build-essential \
        clang-18 \
        llvm-18 \
        lld-18 \
        mlir-18-tools \
        libomp-18-dev \
        gcc-avr \
        avr-libc \
        binutils-avr \
        avrdude \
        simavr \
        python3 \
        entr \
        gawk \
        libasan8 \
        libubsan1 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /usr/src/app

CMD ["sh"]
