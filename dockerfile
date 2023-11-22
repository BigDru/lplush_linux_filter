# syntax=docker/dockerfile:1
FROM arm64v8/gcc:13.2.0
#FROM --platform=$BUILDPLATFORM arm64v8/gcc:13.2.0

# Update base system
RUN apt-get update && \
    apt-get upgrade -y

# Install libcups2-dev and other tools
RUN apt-get install -y libcups2-dev

# Clean up the apt cache to reduce image size
RUN rm -rf /var/lib/apt/lists/*

WORKDIR /build

CMD ["/bin/bash"]
