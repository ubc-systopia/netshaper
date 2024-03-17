#!/bin/bash

# Colors
YELLOW='\033[0;33m'
GREEN='\033[0;32m'
HGREEN='\033[7;32m'
CYAN='\033[0;36m'
RED='\033[0;31m'
OFF='\033[0m'

echo -e "${YELLOW}Installing prerequisites (G++, openSSL-1.1)"
apt-get update && apt-get install -y zstd g++ cmake nlohmann-json3-dev
wget -O - https://mirror.cmt.de/archlinux/core/os/x86_64/openssl-1.1-1.1.1.s-4-x86_64.pkg.tar.zst | unzstd --stdout | tar -xvf -
chmod 644 usr/lib/lib*
cp usr/lib/lib* /usr/lib/x86_64-linux-gnu/
rm -rf usr

echo -e "${YELLOW}Downloading and installing msQUIC"

# Install Microsoft Production repo key
curl -sSL https://packages.microsoft.com/keys/microsoft.asc | tee /etc/apt/trusted.gpg.d/microsoft.asc

# Add Microsoft production repository
add-apt-repository -y https://packages.microsoft.com/ubuntu/22.04/prod/

# Update apt repo
apt-get update

# Install libmsquic
apt-get install -y libmsquic libssl-dev
ln -s /usr/lib/x86_64-linux-gnu/libmsquic.so.2 /usr/lib/x86_64-linux-gnu/libmsquic.so

echo -e "${YELLOW}Downloading and installing ninja-build and cmake"
apt-get install -y ninja-build
# snap install cmake --classic


docker pull amirsabzi/netshaper:peer1-no-shaping
docker pull amirsabzi/netshaper:peer1-shaping


echo -e "${GREEN}Setup complete"
