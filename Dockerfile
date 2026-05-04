FROM php:8.5.4-cli

RUN apt-get update && apt-get install -y --no-install-recommends \
        autoconf \
        bison \
        gcc \
        git \
        libtool \
        make \
        unzip \
    && rm -rf /var/lib/apt/lists/*

RUN curl -fsSL https://github.com/php/pie/releases/latest/download/pie.phar \
        -o /usr/local/bin/pie \
    && chmod +x /usr/local/bin/pie

RUN pie install --skip-enable-extension o0h/astop \
    && echo "extension=astop" > /usr/local/etc/php/conf.d/astop.ini

WORKDIR /work

ENTRYPOINT ["php"]
