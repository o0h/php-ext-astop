FROM php:8.5.4-cli AS builder

RUN apt-get update && apt-get install -y --no-install-recommends \
        autoconf \
        gcc \
        make \
    && rm -rf /var/lib/apt/lists/*

COPY astop.c config.m4 /usr/src/astop/

WORKDIR /usr/src/astop
RUN phpize && ./configure && make


FROM php:8.5.4-cli

COPY --from=builder /usr/src/astop/modules/astop.so /tmp/astop.so
RUN install -m 755 /tmp/astop.so "$(php-config --extension-dir)/astop.so" \
    && rm /tmp/astop.so \
    && echo "extension=astop" > /usr/local/etc/php/conf.d/astop.ini

WORKDIR /work

ENTRYPOINT ["php"]
