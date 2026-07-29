#ifndef APP_INVOICE_H
#define APP_INVOICE_H

/* "Invoices" alt menusunu calistirir: bulundugu dizindeki *.inv dosyalarini
   listeler, kullanici bir rakama basinca o dosyayi sunucuya gonderir
   (Phase 2: artik ekrana basilmiyor), ESC'ye basinca cagiran yere
   (appMain.c) geri doner. */
void invoiceListMenu(void);

/* Faz 1 modunu acar/kapatir.
   0 (varsayilan) = Faz 2 davranisi: secilen fatura sunucuya gonderilir.
   1             = Faz 1 davranisi: secilen faturanin icerigi ekrana basilir
                   ve tam 10 saniye ekranda kalir.
   Isterler listesi iki davranisi da ayri kalemler olarak sayiyor; tek
   surumden ikisini de gosterebilmek icin secenek haline getirildi
   (istemci --phase1 parametresiyle calistirilir). */
void invoiceSetPhase1Mode(int enabled);

#endif /* APP_INVOICE_H */
