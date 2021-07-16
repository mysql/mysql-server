FROM rikorose/gcc-cmake:gcc-8

RUN wget -O boost_1_73_0.tar.gz https://boostorg.jfrog.io/artifactory/main/release/1.73.0/source/boost_1_73_0.tar.gz \
    && tar -xzf boost_1_73_0.tar.gz  -C /usr/local/ \
    && rm boost_1_73_0.tar.gz

#Add Bison
RUN apt-get update \
    && apt-get install -y bison libncurses5-dev libncursesw5-dev libssl-dev perl
