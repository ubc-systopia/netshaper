#FROM ubuntu
FROM python

RUN apt-get update \
    && apt-get install -y tcpdump \
    && rm -rf /var/lib/apt/lists/*

RUN pip install requests

#COPY ./videoClient /root/videoClient
COPY ./video_client.py /root/video_client.py
COPY ./entrypoint.sh /root/entrypoint.sh

WORKDIR /root

ENTRYPOINT ["/bin/bash","/root/entrypoint.sh"]
