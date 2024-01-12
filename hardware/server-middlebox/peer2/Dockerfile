FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update \
    && apt-get install -y \
    curl wget libssl-dev zstd tcpdump \
    && rm -rf /var/lib/apt/lists/*

RUN cd tmp \
    && wget 'http://security.ubuntu.com/ubuntu/pool/main/o/openssl/libssl1.1_1.1.1f-1ubuntu2.19_amd64.deb' \
    && apt-get install ./libssl1.1_1.1.1f-1ubuntu2.19_amd64.deb \
    && rm libssl1.1_1.1.1f-1ubuntu2.19_amd64.deb

RUN curl -sSL https://packages.microsoft.com/keys/microsoft.asc | tee /etc/apt/trusted.gpg.d/microsoft.asc \
    && echo "deb https://packages.microsoft.com/ubuntu/22.04/prod/ jammy main" > /etc/apt/sources.list.d/msquic.list

RUN apt-get update \
    && apt-get install -y \
    libmsquic \
    && rm -rf /var/lib/apt/lists/*

 RUN ln -s /usr/lib/x86_64-linux-gnu/libmsquic.so.2 /usr/lib/x86_64-linux-gnu/libmsquic.so
#RUN ln -s /usr/lib/aarch64-linux-gnu/libmsquic.so.2 /usr/lib/aarch64-linux-gnu/libmsquic.so

COPY ../build/src/example/peer2/peer_2 /root/peer_2
COPY ./entrypoint.sh /root/entrypoint.sh

RUN printf '\n\n\n\n\n\n\n' | \
    openssl req -nodes -new -x509 -keyout /root/server.key -out /root/server.cert

WORKDIR /root

#ENTRYPOINT ["/bin/bash"]
ENTRYPOINT ["/bin/bash","/root/entrypoint.sh"]