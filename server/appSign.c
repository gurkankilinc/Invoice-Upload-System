#include "appSign.h"

#include "mbedtls/sha256.h"
#include "mbedtls/aes.h"

#include <stdio.h>
#include <string.h>

/* Spesifikasyon Step 3.3'te birebir verilen anahtar.
   32 karakter = 256 bit, yani AES-256. */
static const char AES_KEY[] = "12345678901234567890123456789012";

/* AES her zaman 16 baytlik bloklar halinde calisir. */
#define AES_BLOCK_SIZE 16

/* Ham baytlari kucuk harfli hex metne cevirir.
   "static" cunku sadece bu dosyanin ic ihtiyaci; boylece baska bir .c
   dosyasindan cagrilamaz ve ismi projenin geri kalaniyla catismaz.
   Yazarken snprintf kullaniyoruz: sprintf'ten farki, tamponun sonunu asmak
   uzereyken yazmayi kesmesi. */
static int bytesToHex(const unsigned char *bytes, size_t byteCount,
                        char *out, size_t outSize)
{
    size_t i;

    /* Her bayt 2 karakter + sondaki '\0' */
    if (outSize < (byteCount * 2) + 1) {
        return -1;
    }

    for (i = 0; i < byteCount; i++) {
        snprintf(out + (i * 2), outSize - (i * 2), "%02x", bytes[i]);
    }

    out[byteCount * 2] = '\0';
    return 0;
}

int signCalculateHash(const char *data, size_t dataLength,
                        char *hashHexOut, size_t outSize)
{
    unsigned char hash[32]; /* SHA-256 her zaman 32 bayt uretir */

    if (NULL == data || NULL == hashHexOut || SIGN_HASH_HEX_SIZE > outSize) {
        return -1;
    }

    /* Son parametre 0 = SHA-256 (1 verilseydi SHA-224 olurdu) */
    if (0 != mbedtls_sha256((const unsigned char *)data, dataLength, hash, 0)) {
        return -1;
    }

    return bytesToHex(hash, sizeof(hash), hashHexOut, outSize);
}

int signEncryptHash(const char *hashHex, char *signatureHexOut, size_t outSize)
{
    mbedtls_aes_context aes;
    unsigned char encrypted[64]; /* 64 karakterlik hex hash = 64 bayt girdi */
    size_t hashLength;
    size_t offset;
    int result = -1;

    if (NULL == hashHex || NULL == signatureHexOut ||
        SIGN_SIGNATURE_HEX_SIZE > outSize) {
        return -1;
    }

    hashLength = strlen(hashHex);

    /* Sifrelenecek veri, hash'in 64 karakterlik hex metni. 64 sayisi 16'nin
       tam kati oldugu icin (4 blok) padding'e hic gerek kalmiyor - bu da
       imzanin her calistirmada ayni uzunlukta ve tekrar uretilebilir
       olmasini sagliyor. Beklenmedik bir uzunluk gelirse islemi yapmiyoruz. */
    if (sizeof(encrypted) != hashLength) {
        return -1;
    }

    mbedtls_aes_init(&aes);

    if (0 == mbedtls_aes_setkey_enc(&aes, (const unsigned char *)AES_KEY, 256)) {
        result = 0;

        /* ECB modunda her blok bagimsiz sifrelenir; 16'sar bayt ilerleyip
           mbedtls_aes_crypt_ecb'yi blok basina cagiriyoruz. */
        for (offset = 0; offset < hashLength; offset += AES_BLOCK_SIZE) {
            if (0 != mbedtls_aes_crypt_ecb(&aes, MBEDTLS_AES_ENCRYPT,
                                           (const unsigned char *)hashHex + offset,
                                           encrypted + offset)) {
                result = -1;
                break;
            }
        }
    }

    mbedtls_aes_free(&aes);

    if (0 != result) {
        return -1;
    }

    return bytesToHex(encrypted, sizeof(encrypted), signatureHexOut, outSize);
}
