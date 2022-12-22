#!/bin/bash

# Colors
YELLOW='\033[0;33m'
GREEN='\033[0;32m'
HGREEN='\033[7;32m'
CYAN='\033[0;36m'
RED='\033[0;31m'
OFF='\033[0m'

echo -e "${YELLOW}Downloading and installing msQUIC"

# Install Microsoft Production repo key
curl -sSL https://packages.microsoft.com/keys/microsoft.asc | sudo tee /etc/apt/trusted.gpg.d/microsoft.asc

# Add Microsoft production repository
add-apt-repository -y https://packages.microsoft.com/ubuntu/22.04/prod/

# Update apt repo
apt update

# Install libmsquic
apt install -y libmsquic

echo -e "${YELLOW}Downloading and installing ninja-build and cmake"
apt install -y ninja-build
snap install cmake --classic


echo -e "${GREEN}Setup complete"
