FROM ubuntu:20.04

RUN mkdir /temp
ADD . /temp
WORKDIR /temp
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get -y update && apt-get install -y gcc g++ make libpqxx-dev libpugixml-dev libpthread-stubs0-dev libssl-dev postgresql