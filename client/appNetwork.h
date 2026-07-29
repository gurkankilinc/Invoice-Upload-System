#ifndef APP_NETWORK_H
#define APP_NETWORK_H

#include <stddef.h>

/* InvoiceClient'in sunucu ile konusan tarafi (spesifikasyon Phase 2).
   Mesajlar common/appProto.c'deki cerceveleme katmani uzerinden gidiyor. */

/* Kullaniciya IP ve port sorar, sunucuya baglanir ve alma islemleri icin
   30 saniyelik zaman asimi ayarlar.
   1 = baglanti kuruldu, 0 = kurulamadi (cagiran taraf tekrar deneyebilir). */
int networkConnect(void);

/* <Hello>ad soyad sifre</Hello> mesajini gonderir ve sunucunun yanitini
   ekrana basar. 1 = yanit alindi, 0 = alinamadi. */
int networkSendHello(const char *nameSurname, const char *password);

/* Fatura icerigini <Invoice name="...">...</Invoice> mesajiyla gonderir.
   Gelen yaniti ekrana basar; yanit gecerli bir XML belgesiyse imzali dosya
   olarak signed/ klasorune kaydeder.
   30 saniye icinde yanit gelmezse "Upload operation failed" yazar.
   1 = yanit alindi, 0 = alinamadi. */
int networkSendInvoice(const char *fileName,
                         const char *fileContent, size_t contentLength);

/* Acik baglantiyi kapatir. Logout sirasinda cagriliyor. */
void networkDisconnect(void);

#endif /* APP_NETWORK_H */
