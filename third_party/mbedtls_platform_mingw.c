/* mbedtls_config_invoice.h icindeki MBEDTLS_PLATFORM_ZEROIZE_ALT bayraginin
 * karsiligi. MinGW.org 6.3.0 SecureZeroMemory saglamadigi icin bellegi
 * sifirlama isini burada kendimiz yapiyoruz.
 *
 * volatile pointer kullanmamizin sebebi: derleyici "nasil olsa bu bellek bir
 * daha okunmuyor" deyip memset cagrisini tamamen silebilir (dead store
 * elimination). volatile bunu yasaklar, sifirlama gercekten calisir. */

#include "mbedtls/platform_util.h"

#include <stddef.h>

void mbedtls_platform_zeroize(void *buf, size_t len)
{
    if (0 == len) {
        return;
    }

    volatile unsigned char *p = (volatile unsigned char *)buf;
    while (len > 0) {
        *p = 0;
        p++;
        len--;
    }
}
