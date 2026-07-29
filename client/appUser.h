#ifndef APP_USER_H
#define APP_USER_H

/* Giris yapan kullanicinin bilgilerini tutan struct.
   appMain.c bu struct'i olusturup userLogin()'e adresini (pointer) verir,
   fonksiyon da icini doldurur. */
typedef struct {
    char id[64];
    char nameSurname[128];
    char password[64];
} AppUser;

/* Kullanici Id / Sifre sorup dogru girene kadar tekrar tekrar sorar
   (spesifikasyon adim 1.1 - 1.4 ve adim 5).
   Basarili olunca outUser'i doldurur, Audit tablosuna LoginTime kaydeder.
   Veritabani baglantisini da ilk cagrildiginda kendisi acar.

   1 = giris basarili, 0 = girdi bitti (EOF; yonlendirilmis girdiyle
   calisirken uygulamanin duzgun kapanabilmesi icin). */
int userLogin(AppUser *outUser);

/* Audit tablosundaki ilgili kaydin LogoutTime sutununu gunceller. */
void userLogout(const AppUser *user);

#endif /* APP_USER_H */
