#ifndef APP_CONSOLE_H
#define APP_CONSOLE_H

#include <stddef.h>

/* Konsol ciktisinin ortak ayarlari. Hem client hem server kullaniyor.
 *
 * Cozdugu iki sorun:
 *
 * 1) Tamponlama. C kutuphanesi, stdout bir konsol degilse (VS Code'un
 *    entegre terminali, boru, dosyaya yonlendirme) ciktiyi biriktirip toplu
 *    yaziyor. O zaman program bir tus beklerken ekranda yarim cikti kaliyor
 *    ve uygulama donmus gibi gorunuyor. Tamponlamayi kapatiyoruz.
 *
 * 2) Kodlama. Ekrana iki farkli kaynaktan metin basiyoruz:
 *      - sunucudan gelen fatura XML'i  -> UTF-8
 *      - readdir() ile okunan dosya adlari -> sistemin ANSI kod sayfasi
 *    Tek akista iki kodlama olamaz; hangisini secersek digeri bozuk cikar.
 *    Cozum: konsolu UTF-8'e sabitleyip, ANSI kaynakli metinleri yazmadan
 *    once UTF-8'e cevirmek. */

/* Konsolu UTF-8'e alir ve stdout tamponlamasini kapatir.
   Program basinda bir kez cagrilir. Kod sayfasi konsol penceresinin
   ozelligi oldugu icin (surecin degil) cikista eski deger geri yukleniyor. */
void consoleSetup(void);

/* Sistemin ANSI kod sayfasindaki bir metni UTF-8'e cevirir.
   Donen deger out tamponudur; cevirme basarisiz olursa metin oldugu gibi
   kopyalanir, yani cagiran taraf her zaman yazdirilabilir bir sonuc alir. */
const char *consoleToUtf8(const char *ansiText, char *out, size_t outSize);

/* Girdi gercek bir klavyeden mi geliyor?
   1 = evet (normal kullanim), 0 = stdin bir boruya/dosyaya yonlendirilmis
   (otomatik test). Menuler ve sifre okuma bu bilgiye gore davraniyor. */
int consoleIsInteractive(void);

/* Menuler icin tek tus okur.
   Klavyeden calisirken _getch() kullanir: Enter'a basmaya gerek kalmaz,
   spesifikasyonun istedigi davranis budur.
   stdin yonlendirilmisse satirdan tek karakter okur, boylece uygulama
   betikle surulebiliyor (bkz. tests/test_client.py).
   Girdi bittiginde CONSOLE_KEY_EOF doner; cagiran taraf bunu duzgun
   kapanmak icin kullanir, yoksa test sonsuz donguye girerdi. */
#define CONSOLE_KEY_EOF (-1)
int consoleReadKey(void);

/* Sifreyi okur. Klavyeden calisirken karakterleri '*' olarak gosterir;
   stdin yonlendirilmisse duz satir olarak okur. */
void consoleReadPassword(char *buffer, size_t size);

#endif /* APP_CONSOLE_H */
