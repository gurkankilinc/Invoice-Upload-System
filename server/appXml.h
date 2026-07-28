#ifndef APP_XML_H
#define APP_XML_H

#include <stddef.h>

/* Fatura XML'i uzerindeki duzenlemeler. Islemler libxml2 ile yapiliyor.
 *
 * Neden libxml2: onceki surumde imzali dosya, gelen metnin sonuna
 * <invoiceStatus> blogu yapistirilarak uretiliyordu. Fatura dosyalari gercek
 * XML olunca bu yontem gecersiz belge uretti - kok eleman </uploadSystem> ile
 * kapandiktan SONRA icerik geliyordu, halbuki bir XML belgesinde kok elemanin
 * disinda eleman bulunamaz. libxml2 belgeyi agac olarak ayristirdigi icin
 * node'u dogru yere, kokun icine ekleyebiliyoruz. */

/* Gelen fatura XML'ine spesifikasyon Step 3.3'teki invoiceStatus node'unu
   ekler ve imzali belgeyi yeni bir tamponda dondurur:

       <invoiceStatus date="YYYY-MM-DD hh:mm:ss">
           <message>Fatura Kaydedildi</message>
           <signature>...</signature>
       </invoiceStatus>

   Node kok elemanin son cocugu olarak eklenir, kokun namespace'ini devralir.
   Belgede zaten bir invoiceStatus varsa (daha once imzalanmis bir dosya
   tekrar gonderilmisse) eskisi silinip yenisi yazilir.

   Basarili olursa 0 doner, *outXml'e '\0' ile kapatilmis tampon yazar;
   cagiran taraf bunu xml_free_buffer() ile birakmali.
   Gelen veri gecerli XML degilse -1 doner. */
int xml_sign_invoice(const char *invoiceXml, size_t invoiceLength,
                     const char *timestamp, const char *signatureHex,
                     char **outXml, size_t *outLength);

/* xml_sign_invoice'in ayirdigi tamponu birakir. NULL vermek guvenli. */
void xml_free_buffer(char *buffer);

/* Verilen metin ayristirilabilir (well-formed) bir XML belgesi mi?
   1 = evet, 0 = hayir. */
int xml_is_well_formed(const char *xml, size_t length);

/* libxml2'nin ic tablolarini birakir. Program kapanirken bir kez cagrilir. */
void xml_shutdown(void);

#endif /* APP_XML_H */
