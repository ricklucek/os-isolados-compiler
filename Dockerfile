FROM debian:12-slim

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        build-essential \
        entr \
        gawk \
        libasan8 \
        libubsan1 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /usr/src/app

CMD ["sh"]
