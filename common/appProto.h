#ifndef APP_PROTO_H
#define APP_PROTO_H

#include <stddef.h>
#include <winsock2.h>

/* Client ve server arasindaki mesaj cerceveleme (framing) katmani.
 *
 * Neden gerekli: TCP bir "mesaj" protokolu degil, bayt akisi protokolu.
 * send() ile tek seferde gonderilen 900 baytlik bir XML, karsi tarafa iki ayri
 * recv() cagrisinda 500 + 400 olarak gelebilir; ya da iki ayri mesaj tek
 * recv()'de birlesik gelebilir. Onceki kod "bir recv = bir mesaj" varsayiyordu,
 * bu kucuk dosyalarda tesadufen calisiyordu.
 *
 * Cozum: her mesajin onune 4 baytlik uzunluk bilgisi koyuyoruz. Alici once bu
 * 4 bayti okuyor, kac bayt bekleyecegini ogreniyor, sonra tam o kadarini
 * okuyana kadar recv()'i tekrarliyor.
 *
 * Uzunluk big-endian (network byte order) yaziliyor ki farkli mimarideki iki
 * makine arasinda da dogru okunsun - spesifikasyon zaten client ile server'in
 * ayri makinelerde calismasini istiyor.
 *
 * Mesajin ICERIGI degismiyor: telde hala spesifikasyondaki <Hello>...</Hello>
 * ve <Invoice>...</Invoice> metni gidiyor, sadece onunde uzunlugu duruyor. */

/* Tek bir mesaj icin kabul edilen en buyuk boyut (1 MB).
   Bozuk ya da kotu niyetli bir uzunluk alani yuzunden devasa bellek
   ayirmayalim diye ust sinir koyuyoruz. */
#define PROTO_MAX_MESSAGE (1024u * 1024u)

/* Islem sonuclari.
 *
 * Zaman asimi ile baglanti kopmasini AYIRMAK onemli, cunku ikisi farkli sey
 * anlatiyor: zaman asiminda sunucu ayakta ama yavas; kopmada sunucu yok.
 * Kullaniciya dogru mesaji gosterebilmek ve dogru kurtarma adimini
 * atabilmek icin ayri kodlar donduruyoruz. */
#define PROTO_OK      0  /* islem tamamlandi */
#define PROTO_TIMEOUT 1  /* sure doldu, yanit gelmedi */
#define PROTO_CLOSED  2  /* karsi taraf baglantiyi kapatti */
#define PROTO_ERROR   3  /* soket hatasi ya da bozuk mesaj */

/* Mesaji uzunluk onekiyle birlikte gonderir.
   PROTO_OK / PROTO_CLOSED / PROTO_ERROR doner. */
int proto_send(SOCKET sock, const char *message, size_t length);

/* Tam bir mesaj okur. PROTO_OK dondugunde *outMessage'a malloc'lanmis, sonu
   '\0' ile kapatilmis tampon yazar (*outLength = icerik uzunlugu).
   Cagiran taraf tamponu proto_free() ile birakmali.
   Basarisizlikta PROTO_TIMEOUT / PROTO_CLOSED / PROTO_ERROR doner. */
int proto_recv(SOCKET sock, char **outMessage, size_t *outLength);

/* proto_recv'in ayirdigi tamponu birakir. NULL vermek guvenli. */
void proto_free(char *message);

#endif /* APP_PROTO_H */
