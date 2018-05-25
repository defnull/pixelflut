FROM ubuntu:18.04

RUN apt-get update && apt-get install -y maven openjdk-8-jdk

WORKDIR /pixelwar
COPY . .
RUN mvn package
