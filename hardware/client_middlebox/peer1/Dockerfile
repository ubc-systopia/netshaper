FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update \
    && apt-get install -y \
    curl wget libssl-dev zstd tcpdump \
    && rm -rf /var/lib/apt/lists/*

RUN cd tmp \
    && wget 'http://security.ubuntu.com/ubuntu/pool/main/o/openssl/libssl1.1_1.1.1f-1ubuntu2.20_amd64.deb' \
    && apt-get install ./libssl1.1_1.1.1f-1ubuntu2.20_amd64.deb \
    && rm libssl1.1_1.1.1f-1ubuntu2.20_amd64.deb

RUN curl -sSL https://packages.microsoft.com/keys/microsoft.asc | tee /etc/apt/trusted.gpg.d/microsoft.asc \
    && echo "deb https://packages.microsoft.com/ubuntu/22.04/prod/ jammy main" > /etc/apt/sources.list.d/msquic.list

RUN apt-get update \
    && apt-get install -y \
    libmsquic \
    && rm -rf /var/lib/apt/lists/*

 RUN ln -s /usr/lib/x86_64-linux-gnu/libmsquic.so.2 /usr/lib/x86_64-linux-gnu/libmsquic.so
#RUN ln -s /usr/lib/aarch64-linux-gnu/libmsquic.so.2 /usr/lib/aarch64-linux-gnu/libmsquic.so

COPY build/src/example/peer1/peer_1 /root/peer_1
COPY ./entrypoint.sh /root/entrypoint.sh

WORKDIR /root

ENTRYPOINT ["/bin/bash","/root/entrypoint.sh"]