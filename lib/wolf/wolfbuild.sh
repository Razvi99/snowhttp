make clean
./configure --enable-session-ticket --enable-tls13 --disable-oldtls --disable-memory --enable-aesni --enable-intelasm --disable-shared --enable-static --enable-sni --enable-lighty --disable-stunnel --enable-supportedcurves --enable-aesgcm --disable-ocsp --disable-crl --disable-webclient --disable-harden  --enable-fasthugemath C_EXTRA_FLAGS="-DWOLFSSL_STATIC_RSA -DLARGE_STATIC_BUFFERS"
make src/libwolfssl.la

cp src/.libs/libwolfssl.a ../



