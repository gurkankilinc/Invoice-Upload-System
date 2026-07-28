/* InvoiceClient - giris noktasi.
 *
 * Akis: Login (appUser.c) -> sunucu baglantisi (appNetwork.c) ->
 * "Select Operation" menusu -> Invoice List (appInvoice.c) / Logout.
 *
 * Spesifikasyon "Select Operation menusu main'de gosterilsin" dedigi icin
 * menu burada basiliyor, isin kendisi ilgili modullere devrediliyor. */

#include "appUser.h"
#include "appInvoice.h"
#include "appNetwork.h"
#include "appConsole.h"

#include <stdio.h>
#include <string.h>
#include <conio.h>
#include <windows.h>

#define KEY_INVOICE_LIST '1'
#define KEY_LOGOUT       '2'

/* Calisma dizinini exe'nin bulundugu klasore sabitler.
   Neden gerekli: fatura dosyalarini (*.inv) ve upload_system.db'yi calisma
   dizininde ariyoruz. Program hangi klasorden baslatildiysa calisma dizini
   orasi olur; ornegin proje kokunden "client\InvoiceClient.exe" yazilirsa
   calisma dizini proje koku olur ve hicbir fatura bulunamaz.
   Exe kendi klasorunde durdugu icin oraya gecmek dogru sonucu veriyor:
   nereden calistirilirsa calistirilsin (VS Code terminali, cift tiklama,
   baska bir klasor) uygulama kendi verisini buluyor. */
static void set_working_directory_to_exe(void)
{
    char path[MAX_PATH];
    char *lastSeparator;
    DWORD length = GetModuleFileNameA(NULL, path, (DWORD)sizeof(path));

    /* 0 = hata; sizeof(path) kadar donmesi de yolun sigmadigi anlamina gelir */
    if (0 == length || sizeof(path) <= length) {
        return;
    }

    lastSeparator = strrchr(path, '\\');
    if (NULL == lastSeparator) {
        return;
    }

    *lastSeparator = '\0'; /* dosya adini at, geriye klasor kalsin */
    SetCurrentDirectoryA(path);
}

/* Spesifikasyonun istedigi "Select Operation" menusu. */
static void print_select_operation_menu(const AppUser *user)
{
    printf("User : %s\n", user->nameSurname);
    printf("Select Operation\n");
    printf("1 - Invoice List\n");
    printf("2 - Logout\n");
}

/* Komut satirini okur. --phase1 verilirse Faz 1 davranisi acilir:
   sunucuya hic baglanilmaz, secilen faturanin icerigi 10 saniye ekranda
   kalir. Varsayilan davranis Faz 2'dir.
   1 = Faz 1 modu, 0 = normal (Faz 2). */
static int parse_arguments(int argc, char **argv)
{
    int i;

    for (i = 1; i < argc; i++) {
        if (0 == strcmp(argv[i], "--phase1")) {
            return 1;
        }
        printf("Kullanim: %s [--phase1]\n", argv[0]);
        printf("  --phase1 : Faz 1 davranisi (icerigi 10 sn ekranda goster,\n");
        printf("             sunucuya baglanma)\n\n");
    }

    return 0;
}

int main(int argc, char **argv)
{
    AppUser currentUser;
    int phase1Mode;

    set_working_directory_to_exe();
    console_setup();

    phase1Mode = parse_arguments(argc, argv);
    invoice_set_phase1_mode(phase1Mode);

    for (;;) {
        /* Basarili girise kadar burada bekler (appUser.c).
           0 donerse girdi bitmistir, uygulamayi kapatiyoruz. */
        if (0 == user_login(&currentUser)) {
            return 0;
        }

        /* Faz 1 modunda ag katmani hic devreye girmiyor: ne IP/port sorulur
           ne de Hello gonderilir. Faz 1 isterleri sunucudan bagimsiz. */
        if (0 == phase1Mode) {
            /* Spesifikasyon Phase 2: ip ve port login'den SONRA soruluyor.
               Baglanti kurulamazsa bilgiler tekrar sorulur. */
            while (0 == network_connect()) {
                printf("Tekrar deneniyor...\n\n");
            }

            network_send_hello(currentUser.nameSurname, currentUser.password);
        }

        for (;;) {
            int key;

            print_select_operation_menu(&currentUser);

            /* Menude Enter'a gerek yok, tek tusla secim yapiliyor */
            key = console_read_key();

            /* Girdi bitti (yonlendirilmis stdin): duzgunce kapan,
               yoksa otomatik test sonsuz donguye girer. */
            if (CONSOLE_KEY_EOF == key) {
                network_disconnect();
                user_logout(&currentUser);
                return 0;
            }

            if (KEY_INVOICE_LIST == key) {
                invoice_list_menu();
            }
            else if (KEY_LOGOUT == key) {
                printf("Logging out...\n");
                network_disconnect();
                user_logout(&currentUser);
                printf("--------------------------------\n");
                break; /* tekrar "Enter User Id" ekranina don (adim 3) */
            }
        }
    }

    return 0;
}
