#!/bin/bash

# Colors
YELLOW='\033[0;33m'
GREEN='\033[0;32m'
HGREEN='\033[7;32m'
CYAN='\033[0;36m'
RED='\033[0;31m'
OFF='\033[0m'

if ! command -v docker &> /dev/null || ! command -v docker-compose &> /dev/null
then 
	echo -e "${RED}This script requires docker, docker-compose and zsh to be installed${OFF}"
	exit
fi

if ! docker run --rm hello-world > /dev/null 2>&1
then
	echo -e "${RED}Your user does not have permission to run docker${OFF}"
	echo -e "Please ensure this script is executed as root or your user is a part of 'docker' group"
	echo -e "You can add your user to the docker group by running ${YELLOW}sudo usermod -aG docker $USER ${OFF} and then logging out and logging in again"
	exit
fi



if [ ! -d "../../dataset/client/" ]; then
    echo "${RED} the dataset does not exist, downloading the dataset..."
fi