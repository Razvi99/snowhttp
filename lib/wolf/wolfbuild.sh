make clean

./configure --enable-session-ticket --enable-sni --enable-opensslextra --enable-bigcache --disable-oldtls --enable-supportedcurves --disable-memory --enable-aesni --enable-intelasm --disable-shared --enable-static --enable-fasthugemath --enable-fast-rsa

make src/libwolfssl.la

cp src/.libs/libwolfssl.a ../
