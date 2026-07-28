/* InvoiceServer icin sadelestirilmis mbedTLS yapilandirmasi.
 *
 * mbedTLS'in varsayilan mbedtls_config.h dosyasi kutuphanenin tamamini acar;
 * bizim tek ihtiyacimiz spesifikasyondaki iki hesap: icerigin SHA-256 hash'i ve
 * bu hash'in AES-256 ile sifrelenmesi. Bu yuzden -DMBEDTLS_CONFIG_FILE ile
 * varsayilan config yerine bu dosya kullaniliyor; boylece sadece 4 kaynak
 * dosya derlemek yetiyor (sha256.c, aes.c, platform_util.c, constant_time.c).
 *
 * Kapali biraktiklarimizdan iki tanesi bilerek secildi:
 *   MBEDTLS_AESNI_C / MBEDTLS_PADLOCK_C -> islemciye ozel hizlandirma yollari.
 *   Acik olsalardi aesni.c ve padlock.c dosyalarini da derlemek gerekirdi;
 *   fatura boyutundaki veride kazanci olmadigi icin kapali.
 */

#ifndef MBEDTLS_CONFIG_INVOICE_H
#define MBEDTLS_CONFIG_INVOICE_H

/* Spesifikasyon Step 3.2: icerigin HASH'i */
#define MBEDTLS_SHA256_C

/* Spesifikasyon Step 3.3: hash'in AES anahtariyla sifrelenmesi */
#define MBEDTLS_AES_C

/* mbedTLS hassas verileri kullandiktan sonra bellekte sifirlar. Windows'ta
 * bunun icin SecureZeroMemory cagiriyor, ama MinGW.org 6.3.0'in windows.h
 * dosyasinda bu fonksiyon tanimli degil. ALT bayragi mbedTLS'e "bu isi ben
 * yapacagim" demek; karsiligini mbedtls_platform_mingw.c saglıyor.
 * Bu sayede mbedTLS kaynagina hic dokunmuyoruz, guncellemesi kolay kaliyor. */
#define MBEDTLS_PLATFORM_ZEROIZE_ALT

/* mbedTLS'in kendi check_config.h kontrolu Windows'ta bunu zorunlu tutuyor:
 * calloc/free/printf gibi standart kutuphane cagrilarini sarmalayan katman.
 * Karsiligi library/platform.c dosyasidir. */
#define MBEDTLS_PLATFORM_C

#endif /* MBEDTLS_CONFIG_INVOICE_H */
