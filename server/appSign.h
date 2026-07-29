#ifndef APP_SIGN_H
#define APP_SIGN_H

#include <stddef.h>

/* Spesifikasyon Step 3.2 ve 3.3'teki iki hesap.
   Hesaplar mbedTLS ile yapiliyor (bkz. third_party/mbedtls_config_invoice.h);
   onceki surumde Windows'a ozel wincrypt API'si kullaniliyordu. */

/* SHA-256 hash'i icin gereken tampon boyu: 64 hex karakter + '\0' */
#define SIGN_HASH_HEX_SIZE 65

/* Imza icin gereken tampon boyu: 64 baytlik sifreli veri -> 128 hex + '\0' */
#define SIGN_SIGNATURE_HEX_SIZE 129

/* Step 3.2: verinin SHA-256 hash'ini hesaplayip 64 karakterlik kucuk harfli
   hex metin olarak hashHexOut'a yazar.
   0 = basarili, -1 = hata (tampon kucuk ya da hash hesaplanamadi). */
int signCalculateHash(const char *data, size_t dataLength,
                        char *hashHexOut, size_t outSize);

/* Step 3.3: signCalculateHash'in urettigi 64 karakterlik hex hash'i
   spesifikasyondaki AES anahtariyla sifreler, sonucu hex metin olarak yazar.
   0 = basarili, -1 = hata. */
int signEncryptHash(const char *hashHex,
                      char *signatureHexOut, size_t outSize);

#endif /* APP_SIGN_H */
