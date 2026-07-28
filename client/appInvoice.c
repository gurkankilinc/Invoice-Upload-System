#include "appInvoice.h"
#include "appNetwork.h"
#include "appConsole.h"

#include <libxml/parser.h>
#include <libxml/tree.h>

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <windows.h>
#include <conio.h>

/* Klavyede ESC tusuna basildiginda _getch()'in dondurdugu deger */
#define KEY_ESC 27

/* Ayni anda listeleyebilecegimiz en fazla dosya sayisi.
   Tek tus ile secim yaptigimiz icin (1-9) menude en fazla 9 fatura gosteriyoruz. */
#define MAX_INVOICES 9
#define MAX_NAME_LEN 256

/* ".inv" uzantisi. Birden fazla yerde kullanildigi icin sabit olarak duruyor;
   elle 4 yazmak yerine ismi gecsin. */
#define INV_EXTENSION     ".inv"
#define INV_EXTENSION_LEN 4

/* Faz 1'de fatura iceriginin ekranda kalma suresi (isterlerde 10 saniye) */
#define INVOICE_DISPLAY_MS 10000

/* Faz 1 modu acik mi? invoice_set_phase1_mode() ile ayarlaniyor.
   "static": modun degeri sadece bu dosyadan gorulebiliyor, disaridan
   dogrudan degistirilemiyor. */
static int g_phase1Mode = 0;

/* Faturadaki <customer type="..."> degerinin sahis (gercek kisi) anlamina
   gelen hali. Spesifikasyondaki ornek listede kisi adlarinin soyadi buyuk
   yaziliyor ("Hakan USLU"), firma adi ise oldugu gibi kaliyor
   ("Hizli Ticaret"). Ayrimi bu alandan yapiyoruz. */
#define CUSTOMER_TYPE_PERSON "SAHIS"

/* Asagida tanimli; is_person_invoice de dosya okumak icin kullaniyor. */
static int read_invoice_file(const char *fileName, int quiet,
                             char **contentOut, size_t *lengthOut);

/* Fatura bir sahsa mi ait? <customer type="..."> degerine bakar.
   Dosya okunamaz ya da gecerli XML degilse 1 (sahis) varsayiyoruz; boylece
   bilgi yoksa eski davranis (soyadi buyut) korunuyor.

   Dosyayi xmlReadFile ile degil, kendimiz okuyup xmlReadMemory ile
   ayristiriyoruz: xmlReadFile Windows'ta yol adini UTF-8 kabul ediyor, oysa
   readdir() dosya adlarini sistemin ANSI kod sayfasinda veriyor. Turkce
   karakterli bir dosya adi ("Hizli Ticaret.inv") bu yuzden acilamazdi. */
static int is_person_invoice(const char *fileName)
{
    xmlDocPtr doc;
    xmlNodePtr root;
    xmlNodePtr child;
    char *content;
    size_t contentLength;
    int isPerson = 1;

    if (0 == read_invoice_file(fileName, 1, &content, &contentLength)) {
        return 1;
    }

    doc = xmlReadMemory(content, (int)contentLength, "invoice.xml", NULL,
                        XML_PARSE_NOERROR | XML_PARSE_NOWARNING);
    free(content);

    if (NULL == doc) {
        return 1;
    }

    root = xmlDocGetRootElement(doc);
    if (NULL != root) {
        for (child = root->children; NULL != child; child = child->next) {
            if (XML_ELEMENT_NODE == child->type &&
                0 == xmlStrcmp(child->name, BAD_CAST "customer")) {
                xmlChar *type = xmlGetProp(child, BAD_CAST "type");

                if (NULL != type) {
                    isPerson = (0 == xmlStrcmp(type, BAD_CAST CUSTOMER_TYPE_PERSON));
                    xmlFree(type);
                }
                break;
            }
        }
    }

    xmlFreeDoc(doc);
    return isPerson;
}

/* Calisma dizinindeki ".inv" ile biten dosyalarin adlarini fileNames dizisine
   yazar, kac tane bulundugunu dondurur. */
static int scan_invoice_files(char fileNames[][MAX_NAME_LEN])
{
    DIR *dir = opendir(".");
    struct dirent *entry;
    int count = 0;

    if (NULL == dir) {
        return 0;
    }

    while (MAX_INVOICES > count && NULL != (entry = readdir(dir))) {
        size_t length = strlen(entry->d_name);

        /* dosya adi ".inv" ile mi bitiyor? */
        if (INV_EXTENSION_LEN < length &&
            0 == strcmp(entry->d_name + length - INV_EXTENSION_LEN, INV_EXTENSION)) {
            /* strncpy en fazla n karakter kopyalar, yani uzun bir dosya adi
               diziyi tasiramaz. n karakterin tamami dolarsa sonuna '\0'
               koymadigi icin son bayti elle sifirliyoruz. strcpy'de boyle bir
               sinir olmadigindan uzun bir isim tamponu tasirirdi. */
            strncpy(fileNames[count], entry->d_name, MAX_NAME_LEN - 1);
            fileNames[count][MAX_NAME_LEN - 1] = '\0';
            count++;
        }
    }

    closedir(dir);
    return count;
}

static void print_invoice_menu(char fileNames[][MAX_NAME_LEN], int count)
{
    int i;

    printf("Invoices\n");

    for (i = 0; i < count; i++) {
        char displayName[MAX_NAME_LEN];
        char utf8Name[MAX_NAME_LEN * 2]; /* UTF-8'de bir karakter 4 bayta kadar cikabilir */
        size_t nameLength = strlen(fileNames[i]) - INV_EXTENSION_LEN;
        char *lastSpace;

        memcpy(displayName, fileNames[i], nameLength);
        displayName[nameLength] = '\0';

        /* Spesifikasyondaki ornekteki gibi soyadi (son kelimeyi) buyuk harfe
           ceviriyoruz: "Hakan Uslu" -> "Hakan USLU".
           Firma faturalarinda bu yapilmiyor: ornekteki ucuncu satir
           "Hizli Ticaret" oldugu gibi duruyor. */
        lastSpace = strrchr(displayName, ' ');
        if (NULL != lastSpace && 0 != is_person_invoice(fileNames[i])) {
            size_t j;
            for (j = (size_t)(lastSpace - displayName) + 1; j < nameLength; j++) {
                displayName[j] = (char)toupper((unsigned char)displayName[j]);
            }
        }

        /* Dosya adi diskten ANSI kod sayfasinda geliyor, konsol ise UTF-8'e
           ayarli (bkz. common/appConsole.c). Yazmadan once cevirmezsek
           Turkce karakterli bir dosya adi bozuk gorunur. */
        printf("%d - %s\n", i + 1,
               console_to_utf8(displayName, utf8Name, sizeof(utf8Name)));
    }

    printf("Press ESC to return main menu\n");
}

/* Dosyanin tamamini bellege okur. Basarili olursa 1 doner; *contentOut'a
   free() ile birakilacak tampon, *lengthOut'a bayt sayisi yazar.

   quiet: 1 verilirse hata mesaji basilmaz. Menu cizilirken her dosya icin
   cagriliyoruz; orada okunamayan bir dosya yuzunden ekrani hata mesajiyla
   doldurmak istemiyoruz.

   Neden sabit boyutlu dizi degil: fatura dosyalari XML ve boyutlari degisken.
   Once fseek/ftell ile gercek boyutu ogrenip tam o kadar yer ayiriyoruz,
   boylece buyuk bir fatura sessizce kesilmiyor. */
static int read_invoice_file(const char *fileName, int quiet,
                             char **contentOut, size_t *lengthOut)
{
    /* "rb": dosyayi bayt bayt, oldugu gibi okuyoruz. Metin modunda Windows
       "\r\n" dizilerini "\n" yapar; o zaman gonderdigimiz icerik diskteki
       dosyadan farkli olur ve hash de farkli cikardi. */
    FILE *fp = fopen(fileName, "rb");
    long fileSize;
    char *buffer;
    size_t bytesRead;

    *contentOut = NULL;
    *lengthOut = 0;

    if (NULL == fp) {
        if (0 == quiet) {
            printf("Dosya acilamadi: %s\n", fileName);
        }
        return 0;
    }

    fseek(fp, 0, SEEK_END);
    fileSize = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (0 > fileSize) {
        fclose(fp);
        if (0 == quiet) {
            printf("Dosya boyutu okunamadi: %s\n", fileName);
        }
        return 0;
    }

    buffer = (char *)malloc((size_t)fileSize + 1);
    if (NULL == buffer) {
        fclose(fp);
        if (0 == quiet) {
            printf("Bellek ayrilamadi\n");
        }
        return 0;
    }

    bytesRead = fread(buffer, 1, (size_t)fileSize, fp);
    fclose(fp);

    buffer[bytesRead] = '\0';

    *contentOut = buffer;
    *lengthOut = bytesRead;
    return 1;
}

/* Faz 1 davranisi: secilen faturanin icerigi ekrana basilir ve tam 10 saniye
   ekranda kalir, sonra menuye donulur.
   Faz 2 bu davranisin yerine dosyayi sunucuya gondermeyi getirdi; ancak
   isterler listesi Faz 1 maddesini de ayri bir kalem olarak sayiyor, bu
   yuzden davranis --phase1 secenegiyle hala gosterilebiliyor. */
static void show_invoice_content(const char *fileName)
{
    char *content;
    size_t contentLength;

    if (0 == read_invoice_file(fileName, 0, &content, &contentLength)) {
        Sleep(2000);
        return;
    }

    printf("\n%s\n", content);
    free(content);

    Sleep(INVOICE_DISPLAY_MS); /* isterlerdeki 10 saniye */
}

/* Faz 2 davranisi: secilen fatura ekrana basilmiyor, sunucuya gonderiliyor. */
static void upload_invoice(const char *fileName)
{
    char *content;
    size_t contentLength;

    if (0 == read_invoice_file(fileName, 0, &content, &contentLength)) {
        Sleep(2000);
        return;
    }

    /* "... file has been sent, waiting for registration" bilgilendirmesi
       network_send_invoice icinde, gonderim ile yanit beklemesi arasinda
       yazdiriliyor - mesajin anlami zaten orayi tarif ediyor. */
    network_send_invoice(fileName, content, contentLength);
    free(content);

    /* Yaniti okuyabilmesi icin kullaniciya sure taniyoruz, sonra menu
       yeniden geliyor (spesifikasyon: "Invoices menu is shown again"). */
    printf("\nDevam etmek icin bir tusa basin...\n");
    console_read_key();
}

void invoice_list_menu(void)
{
    char fileNames[MAX_INVOICES][MAX_NAME_LEN];
    int count;

    for (;;) {
        int key;

        count = scan_invoice_files(fileNames);
        print_invoice_menu(fileNames, count);

        key = console_read_key();

        if (KEY_ESC == key || CONSOLE_KEY_EOF == key) {
            return; /* Select Operation menusune geri don */
        }

        if ('1' <= key && '9' >= key) {
            int selection = key - '0';

            if (selection <= count) {
                if (0 != g_phase1Mode) {
                    show_invoice_content(fileNames[selection - 1]);
                } else {
                    upload_invoice(fileNames[selection - 1]);
                }
                continue;
            }
        }

        printf("record not found\n");
        Sleep(2000);
    }
}

void invoice_set_phase1_mode(int enabled)
{
    g_phase1Mode = enabled;
}
