FROM ubuntu

COPY ./wrk2/wrk /root/wrk
COPY ./entrypoint.sh /root/entrypoint.sh

WORKDIR /root

ENTRYPOINT [ "/bin/bash", "/root/entrypoint.sh" ]