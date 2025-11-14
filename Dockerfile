FROM ubuntu:18.04

RUN apt-get update && apt-get install -y \
    sudo \
    git  \
    cmake  \
    gcc-8   \
    g++-8    \
    libsqlite3-dev  \
    qt5-default   \ 
    ninja-build \
    libaudit-dev

RUN update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-8 100 && \
    update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-8 100

CMD ["/bin/bash"]