#changed from debian to ubuntu
FROM debian:buster-slim

# add our user and group first to make sure their IDs get assigned consistently, regardless of whatever dependencies get added
RUN groupadd -r mysql && useradd -r -g mysql mysql

RUN apt-get update && apt-get install -y --no-install-recommends gnupg dirmngr && rm -rf /var/lib/apt/lists/*

# add gosu for easy step-down from root
# https://github.com/tianon/gosu/releases
ENV GOSU_VERSION 1.12
RUN set -eux; \
    savedAptMark="$(apt-mark showmanual)"; \
    apt-get update; \
    apt-get install -y --no-install-recommends ca-certificates wget; \
    rm -rf /var/lib/apt/lists/*; \
    dpkgArch="$(dpkg --print-architecture | awk -F- '{ print $NF }')"; \
    wget -O /usr/local/bin/gosu "https://github.com/tianon/gosu/releases/download/$GOSU_VERSION/gosu-$dpkgArch"; \
    wget -O /usr/local/bin/gosu.asc "https://github.com/tianon/gosu/releases/download/$GOSU_VERSION/gosu-$dpkgArch.asc"; \
    export GNUPGHOME="$(mktemp -d)"; \
    gpg --batch --keyserver hkps://keys.openpgp.org --recv-keys B42F6819007F00F88E364FD4036A9C25BF357DD4; \
    gpg --batch --verify /usr/local/bin/gosu.asc /usr/local/bin/gosu; \
    gpgconf --kill all; \
    rm -rf "$GNUPGHOME" /usr/local/bin/gosu.asc; \
    apt-mark auto '.*' > /dev/null; \
    [ -z "$savedAptMark" ] || apt-mark manual $savedAptMark > /dev/null; \
    apt-get purge -y --auto-remove -o APT::AutoRemove::RecommendsImportant=false; \
    chmod +x /usr/local/bin/gosu; \
    gosu --version; \
    gosu nobody true

RUN mkdir /docker-entrypoint-initdb.d

# set a timezone with tzdata this is used by mysql fails if not set
ARG DEBIAN_FRONTEND=noninteractive
ENV TZ=Europe/London
RUN apt-get update && apt-get install -y --no-install-recommends \
    # for MYSQL_RANDOM_ROOT_PASSWORD
    pwgen \
    # for mysql_ssl_rsa_setup
    openssl \
    # FATAL ERROR: please install the following Perl modules before executing /usr/local/mysql/scripts/mysql_install_db:
    # File::Basename
    # File::Copy
    # Sys::Hostname
    # Data::Dumper
    libaio1 \
    libstdc++6 \
    tzdata \
    libnuma-dev \
    libncurses5-dev \
    libncursesw5-dev \
    perl \
    # install "xz-utils" for .sql.xz docker-entrypoint-initdb.d files
    xz-utils \
    && apt-get dist-upgrade -y \
    && rm -rf /var/lib/apt/lists/*

ENV MYSQL_MAJOR 8.0
ENV MYSQL_VERSION 8.0.25-1debian10

# bust cache
ADD "https://www.random.org/cgi-bin/randbyte?nbytes=10&format=h" skipcache
# Copy package from build
COPY ./build/mysql-8.0.25-linux-x86_64.tar.gz /mysql.tar.gz
# follow inustruction from https://dev.mysql.com/doc/mysql-installation-excerpt/8.0/en/binary-installation.html
#COPY mysql-8.0.25-linux-x86_64.tar.gz / 
RUN mkdir /usr/local/mysql \
    && tar xvf /mysql.tar.gz  -C /usr/local/mysql --strip-components=1 \
    && rm /mysql.tar.gz \
    && chown -R mysql:mysql /usr/local/mysql \
    && cd /usr/local/mysql \
    && mkdir mysql-files\
    && chown mysql:mysql mysql-files\
    && chmod 750 mysql-files



# the "/var/lib/mysql" stuff here is because the mysql-server postinst doesn't have an explicit way to disable the mysql_install_db codepath besides having a database already "configured" (ie, stuff in /var/lib/mysql/mysql)
# also, we set debconf keys to make APT a little quieter
RUN apt-get update \
    && apt-get install -y \
    && rm -rf /var/lib/apt/lists/* \
    && rm -rf /var/lib/mysql && mkdir -p /var/lib/mysql /var/run/mysqld \
    && chown -R mysql:mysql /var/lib/mysql /var/run/mysqld \
    # ensure that /var/run/mysqld (used for socket and lock files) is writable regardless of the UID our mysqld instance ends up having at runtime
    && chmod 1777 /var/run/mysqld /var/lib/mysql

VOLUME /var/lib/mysql

# Config files
COPY config/ /etc/mysql/
COPY config/my.cnf /etc/mysql/my.cnf
COPY docker-entrypoint.sh /usr/local/bin/
RUN ln -s usr/local/bin/docker-entrypoint.sh /entrypoint.sh # backwards compat
# make script executubale 
RUN chmod 777 /usr/local/bin/docker-entrypoint.sh \
    && ln -s /usr/local/bin/docker-entrypoint.sh /


ENV PATH="${PATH}:/usr/local/mysql/bin"

ENTRYPOINT ["docker-entrypoint.sh"]

EXPOSE 3306 33060
CMD ["mysqld"]
