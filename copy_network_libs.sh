#!/bin/bash
# Qt6Network bağımlılık zinciri için gerekli kütüphaneleri kopyalar.
DEST=$1
[ -z "$DEST" ] && exit 1

for lib in \
    /usr/lib/libngtcp2.so* \
    /usr/lib/libngtcp2_crypto_ossl.so* \
    /usr/lib/libnghttp2.so* \
    /usr/lib/libnghttp3.so*; do
    [ -f "$lib" ] && cp -Lf "$lib" "$DEST/" && echo "Copied: $(basename $lib)"
done
