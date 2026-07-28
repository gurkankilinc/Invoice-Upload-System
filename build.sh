#!/bin/sh
# InvoiceClient ve InvoiceServer derleme betigi (MinGW gcc).
#
# Kullanim (proje kokunden):
#   sh build.sh          -> ikisini de derler
#   sh build.sh client   -> sadece client
#   sh build.sh server   -> sadece server
#
# Ciktilar:
#   client/InvoiceClient.exe
#   server/InvoiceServer.exe
#
# Exe'ler bilerek kendi veri klasorlerinin yanina konuyor: client'in fatura
# dosyalarina (*.inv) ve upload_system.db'ye erismesi gerekiyor, server ise
# tek basina calisiyor. Boylece her iki klasor de kendi basina tasinabilir.

set -e

MBEDTLS_SRC="third_party/mbedtls/library"
MBEDTLS_OBJS="
  $MBEDTLS_SRC/sha256.c
  $MBEDTLS_SRC/aes.c
  $MBEDTLS_SRC/platform.c
  $MBEDTLS_SRC/platform_util.c
  $MBEDTLS_SRC/constant_time.c
  third_party/mbedtls_platform_mingw.c
"

LIBXML2_A="third_party/libxml2/win32/bin.mingw/libxml2.a"

COMMON_FLAGS="-O2 -Wall -DLIBXML_STATIC -Icommon -Ithird_party -Ithird_party/libxml2/include"

build_client() {
    echo "InvoiceClient derleniyor..."
    gcc $COMMON_FLAGS \
        -Iclient -Ithird_party/sqlite3 \
        -o client/InvoiceClient.exe \
        client/appMain.c client/appUser.c client/appInvoice.c client/appNetwork.c \
        common/appProto.c common/appConsole.c \
        third_party/sqlite3/sqlite3.c \
        "$LIBXML2_A" -lws2_32
    echo "  -> client/InvoiceClient.exe"
}

build_server() {
    echo "InvoiceServer derleniyor..."
    gcc $COMMON_FLAGS \
        -Iserver -Ithird_party/mbedtls/include \
        -DMBEDTLS_CONFIG_FILE='"mbedtls_config_invoice.h"' \
        -o server/InvoiceServer.exe \
        server/appServer.c server/appSign.c server/appXml.c \
        common/appProto.c common/appConsole.c \
        $MBEDTLS_OBJS \
        "$LIBXML2_A" -lws2_32
    echo "  -> server/InvoiceServer.exe"
}

case "${1:-all}" in
    client) build_client ;;
    server) build_server ;;
    all)    build_client; build_server ;;
    *)      echo "Kullanim: sh build.sh [client|server|all]"; exit 1 ;;
esac

echo "Tamam."
