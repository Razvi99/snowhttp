make clean

./configure --enable-session-ticket --enable-bigcache --enable-opensslextra --enable-supportedcurves --enable-tls13 --disable-oldtls --disable-memory --enable-aesni --enable-intelasm --disable-shared --enable-static --enable-fasthugemath --enable-fast-rsa CFLAGS="-O3 -march=native"

make src/libwolfssl.la

cp src/.libs/libwolfssl.a ../
