FROM ubuntu:22.04

RUN apt-get update && apt-get install -y wget cmake zstd g++ nlohmann-json3-dev software-properties-common curl ninja-build git libboost-program-options-dev sudo pkg-config

RUN wget https://mirror.cmt.de/archlinux/core/os/x86_64/openssl-1.1-1.1.1.w-2-x86_64.pkg.tar.zst  -O /tmp/openssl-1.1-1.1.1.pkg.tar.zst
RUN unzstd /tmp/openssl-1.1-1.1.1.pkg.tar.zst --stdout | tar -xvf - -C /tmp
RUN chmod 644 /tmp/usr/lib/lib*
RUN cp -r /tmp/usr/lib/* /usr/lib/x86_64-linux-gnu/

# Install Microsoft Production repo key
RUN curl -sSL https://packages.microsoft.com/keys/microsoft.asc | tee /etc/apt/trusted.gpg.d/microsoft.asc

# Add Microsoft Production repo
RUN add-apt-repository -y https://packages.microsoft.com/ubuntu/22.04/prod/

# Update apt repo
RUN apt update
RUN apt-get install -y libmsquic libssl-dev
RUN ln -s /usr/lib/x86_64-linux-gnu/libmsquic.so.2 /usr/lib/x86_64-linux-gnu/libmsquic.so
