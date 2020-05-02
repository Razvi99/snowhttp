make clean
./configure --disable-memory --enable-aesni --enable-intelasm --disable-shared --enable-static --enable-sni --enable-lighty --disable-stunnel --enable-supportedcurves --enable-aesgcm --disable-ocsp --disable-crl --disable-webclient --disable-harden  --enable-fasthugemath --enable-debug C_EXTRA_FLAGS="-DWOLFSSL_STATIC_RSA -DLARGE_STATIC_BUFFERS" 
make src/libwolfssl.la


