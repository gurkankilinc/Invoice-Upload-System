/* InvoiceServer - Phase 2
 *
 * Client'tan gelen Hello ve Invoice mesajlarini karsilar; Invoice mesajinda
 * icerigin SHA-256 hash'ini alir, hash'i AES ile sifreler ve faturaya
 * <invoiceStatus> node'unu ekleyip imzali XML'i geri gonderir.
 *
 * Kripto islemleri appSign.c'de (mbedTLS), XML islemleri appXml.c'de
 * (libxml2), mesaj cerceveleme common/appProto.c'de. */

#include "appSign.h"
#include "appXml.h"
#include "appProto.h"
#include "appConsole.h"

#include <winsock2.h>
#include <ws2tcpip.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <windows.h>

#define DEFAULT_PORT 54000

/* Mesaj etiketleri - spesifikasyonda birebir bu sekilde geciyor. */
#define TAG_HELLO_OPEN    "<Hello>"
#define TAG_HELLO_CLOSE   "</Hello>"
#define TAG_INVOICE_OPEN  "<Invoice"
#define TAG_INVOICE_CLOSE "</Invoice>"
#define MSG_READY         "<Response>I am ready</Response>"

/* Spesifikasyon "Upload operation failed" testini isterken sunucu tarafina
   gecici bir gecikme eklenmesini soyluyor. Bunu koda gomup sonra silmek
   yerine komut satiri secenegi yaptik: --delay <saniye>.
   0 = gecikme yok (normal calisma). */
static int g_responseDelaySeconds = 0;

/* Sunucunun dinledigi IP'yi bulup ekrana yazar (spesifikasyon acilista
   "ip, port" bilgisinin gosterilmesini istiyor). */
static void print_listen_address(int port)
{
    char hostName[256];
    struct hostent *host;
    char ipText[64];

    /* strncpy: kaynak metin hedeften uzunsa en fazla n karakter kopyalar,
       yani tasma olmaz. Ama n karakter doluysa sonuna '\0' KOYMAZ; bu yuzden
       son bayti elle sifirliyoruz. strcpy'de boyle bir sinir hic yok, uzun
       kaynak tamponu tasirirdi. */
    strncpy(ipText, "127.0.0.1", sizeof(ipText) - 1);
    ipText[sizeof(ipText) - 1] = '\0';

    if (0 == gethostname(hostName, sizeof(hostName))) {
        host = gethostbyname(hostName);

        if (NULL != host && NULL != host->h_addr_list[0]) {
            struct in_addr addr;
            memcpy(&addr, host->h_addr_list[0], sizeof(struct in_addr));
            strncpy(ipText, inet_ntoa(addr), sizeof(ipText) - 1);
            ipText[sizeof(ipText) - 1] = '\0';
        }
    }

    printf("InvoiceServer dinlemede\n");
    printf("  IP   : %s\n", ipText);
    printf("  Port : %d\n", port);

    if (0 < g_responseDelaySeconds) {
        printf("  TEST : yanit %d saniye geciktirilecek\n", g_responseDelaySeconds);
    }

    printf("\n");
}

/* <Hello>Ad Soyad sifre</Hello> mesajindan sadece ad soyad kismini alir.
   Son bosluktan oncesi ad soyad, sonrasi sifre kabul ediliyor (ad soyadin
   icinde bosluk olabilir, sifrede olamaz).
   Spesifikasyon sadece kullanici adinin gosterilmesini istiyor; sifre
   hicbir yere yazilmiyor.
   1 = ad soyad cikarildi, 0 = mesaj beklenen bicimde degil. */
static int extract_user_name(const char *message, char *nameOut, size_t outSize)
{
    const char *contentStart;
    const char *contentEnd;
    const char *lastSpace = NULL;
    const char *scan;
    size_t nameLength;

    if (NULL == message || NULL == nameOut || 0 == outSize) {
        return 0;
    }

    contentStart = message + strlen(TAG_HELLO_OPEN);
    contentEnd = strstr(contentStart, TAG_HELLO_CLOSE);
    if (NULL == contentEnd) {
        return 0;
    }

    /* Sifreyi ayiran bosluk, kapanis etiketinden onceki son bosluk olmali */
    for (scan = contentStart; scan < contentEnd; scan++) {
        if (' ' == *scan) {
            lastSpace = scan;
        }
    }

    if (NULL == lastSpace) {
        return 0;
    }

    nameLength = (size_t)(lastSpace - contentStart);
    if (nameLength >= outSize) {
        nameLength = outSize - 1;
    }

    memcpy(nameOut, contentStart, nameLength);
    nameOut[nameLength] = '\0';
    return 1;
}

/* <Invoice name="dosya.inv">...XML...</Invoice> mesajini parcalarina ayirir.
   Dosya adini nameOut'a yazar; icerigin baslangic adresini ve uzunlugunu
   dondurur (icerik kopyalanmiyor, mesaj tamponunun icini gosteriyor).
   1 = ayristirildi, 0 = mesaj beklenen bicimde degil. */
static int split_invoice_message(char *message, size_t messageLength,
                                 char *nameOut, size_t nameOutSize,
                                 const char **contentOut, size_t *contentLengthOut)
{
    char *nameStart;
    char *nameEnd;
    char *tagEnd;
    size_t nameLength;
    size_t closeTagLength = strlen(TAG_INVOICE_CLOSE);
    size_t contentStartOffset;

    nameStart = strstr(message, "name=\"");
    if (NULL == nameStart) {
        return 0;
    }
    nameStart += strlen("name=\"");

    nameEnd = strchr(nameStart, '"');
    if (NULL == nameEnd) {
        return 0;
    }

    nameLength = (size_t)(nameEnd - nameStart);
    if (nameLength >= nameOutSize) {
        nameLength = nameOutSize - 1;
    }
    memcpy(nameOut, nameStart, nameLength);
    nameOut[nameLength] = '\0';

    /* Acilis etiketini kapatan '>' isaretini, oznitelik degerinin
       tirnagindan SONRA ariyoruz. Fatura icerigi de XML oldugu icin icinde
       bol bol '>' var; bastan aramak yanlis yeri bulurdu. */
    tagEnd = strchr(nameEnd, '>');
    if (NULL == tagEnd) {
        return 0;
    }

    contentStartOffset = (size_t)(tagEnd - message) + 1;

    /* Mesajin tam uzunlugunu proto katmanindan biliyoruz, dolayisiyla
       kapanis etiketini aramak yerine sondan kesiyoruz. Icerikte
       "</Invoice>" gecse bile dogru calisir. */
    if (messageLength < contentStartOffset + closeTagLength) {
        return 0;
    }

    if (0 != strcmp(message + messageLength - closeTagLength, TAG_INVOICE_CLOSE)) {
        return 0;
    }

    *contentOut = message + contentStartOffset;
    *contentLengthOut = messageLength - contentStartOffset - closeTagLength;
    return 1;
}

/* Su anki tarih/saati spesifikasyonun istedigi "YYYY-MM-DD hh:mm:ss"
   bicimine cevirir. */
static void format_now(char *out, size_t outSize)
{
    time_t now = time(NULL);
    struct tm *parts = localtime(&now);

    strftime(out, outSize, "%Y-%m-%d %H:%M:%S", parts);
}

/* Invoice mesajini isler ve imzali XML'i client'a gonderir. */
static void handle_invoice(SOCKET clientSocket, char *message, size_t messageLength)
{
    char fileName[256];
    char fileNameUtf8[512]; /* ekrana yazarken kullanilan UTF-8 hali */
    const char *content;
    size_t contentLength;
    char hashHex[SIGN_HASH_HEX_SIZE];
    char signatureHex[SIGN_SIGNATURE_HEX_SIZE];
    char timestamp[32];
    char *signedXml = NULL;
    size_t signedLength = 0;

    if (0 == split_invoice_message(message, messageLength,
                                   fileName, sizeof(fileName),
                                   &content, &contentLength)) {
        const char *error = "<Response>Invoice mesaji cozumlenemedi</Response>";
        printf("Invoice mesaji beklenen bicimde degil, islenmedi\n\n");
        proto_send(clientSocket, error, strlen(error));
        return;
    }

    /* Spesifikasyonun istedigi bilgilendirme mesaji.
       Dosya adi client'tan ANSI kod sayfasinda geliyor, konsolumuz UTF-8;
       yazmadan once ceviriyoruz (bkz. common/appConsole.c). */
    printf("%s file has been sent, waiting for registration\n",
           console_to_utf8(fileName, fileNameUtf8, sizeof(fileNameUtf8)));

    /* Step 3.2: icerigin hash'i */
    if (0 != sign_calculate_hash(content, contentLength, hashHex, sizeof(hashHex))) {
        const char *error = "<Response>Hash hesaplanamadi</Response>";
        printf("  HATA : HASH hesaplanamadi\n\n");
        proto_send(clientSocket, error, strlen(error));
        return;
    }

    /* Step 3.3: hash'in AES ile sifrelenmesi */
    if (0 != sign_encrypt_hash(hashHex, signatureHex, sizeof(signatureHex))) {
        const char *error = "<Response>Imza uretilemedi</Response>";
        printf("  HATA : IMZA uretilemedi\n\n");
        proto_send(clientSocket, error, strlen(error));
        return;
    }

    printf("  HASH : %s\n", hashHex);
    printf("  IMZA : %s\n", signatureHex);

    format_now(timestamp, sizeof(timestamp));

    if (0 != xml_sign_invoice(content, contentLength, timestamp, signatureHex,
                              &signedXml, &signedLength)) {
        const char *error = "<Response>Gelen fatura gecerli XML degil</Response>";
        printf("  HATA : gelen icerik gecerli bir XML belgesi degil\n\n");
        proto_send(clientSocket, error, strlen(error));
        return;
    }

    /* Zaman asimi testi icin gecikme (--delay). Normalde 0. */
    if (0 < g_responseDelaySeconds) {
        printf("  TEST : yanit %d saniye bekletiliyor...\n", g_responseDelaySeconds);
        Sleep((DWORD)g_responseDelaySeconds * 1000);
    }

    if (0 == proto_send(clientSocket, signedXml, signedLength)) {
        printf("  Imzali XML gonderildi (%u bayt)\n\n", (unsigned)signedLength);
    } else {
        printf("  Imzali XML gonderilemedi\n\n");
    }

    xml_free_buffer(signedXml);
}

/* Tek bir client baglantisini, kapanana kadar mesaj mesaj isler. */
static void serve_client(SOCKET clientSocket)
{
    for (;;) {
        char *message = NULL;
        size_t messageLength = 0;

        if (0 != proto_recv(clientSocket, &message, &messageLength)) {
            break; /* client kapatti ya da hata olustu */
        }

        /* Step 3.1: istek tipine bak */
        if (0 == strncmp(message, TAG_HELLO_OPEN, strlen(TAG_HELLO_OPEN))) {
            char nameSurname[128];

            if (0 != extract_user_name(message, nameSurname, sizeof(nameSurname))) {
                printf("Kullanici: %s\n\n", nameSurname);
            } else {
                printf("Hello mesaji beklenen bicimde degil\n\n");
            }

            proto_send(clientSocket, MSG_READY, strlen(MSG_READY));
        }
        else if (0 == strncmp(message, TAG_INVOICE_OPEN, strlen(TAG_INVOICE_OPEN))) {
            handle_invoice(clientSocket, message, messageLength);
        }
        else {
            printf("Bilinmeyen mesaj (%u bayt)\n\n", (unsigned)messageLength);
        }

        proto_free(message);
    }
}

/* --port ve --delay seceneklerini okur. Taninmayan secenekte 0 doner. */
static int parse_arguments(int argc, char **argv, int *portOut)
{
    int i;

    *portOut = DEFAULT_PORT;

    for (i = 1; i < argc; i++) {
        if (0 == strcmp(argv[i], "--port") && (i + 1) < argc) {
            *portOut = atoi(argv[++i]);
        }
        else if (0 == strcmp(argv[i], "--delay") && (i + 1) < argc) {
            g_responseDelaySeconds = atoi(argv[++i]);
        }
        else {
            printf("Kullanim: %s [--port <port>] [--delay <saniye>]\n", argv[0]);
            printf("  --delay : zaman asimi testi icin yaniti geciktirir\n");
            return 0;
        }
    }

    return 1;
}

int main(int argc, char **argv)
{
    WSADATA wsaData;
    SOCKET listenSocket;
    struct sockaddr_in serverAddr;
    int port;

    /* Tamponsuz cikti + UTF-8 konsol (bkz. common/appConsole.c) */
    console_setup();

    if (0 == parse_arguments(argc, argv, &port)) {
        return 1;
    }

    if (0 != WSAStartup(MAKEWORD(2, 2), &wsaData)) {
        printf("WSAStartup basarisiz\n");
        return 1;
    }

    listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (INVALID_SOCKET == listenSocket) {
        printf("Soket olusturulamadi: %d\n", WSAGetLastError());
        WSACleanup();
        return 1;
    }

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY; /* tum ag arayuzlerinden kabul et */
    serverAddr.sin_port = htons((u_short)port);

    if (SOCKET_ERROR == bind(listenSocket, (struct sockaddr *)&serverAddr,
                             sizeof(serverAddr))) {
        int error = WSAGetLastError();

        printf("Bind basarisiz: %d\n", error);

        /* 10048 = WSAEADDRINUSE. En sik karsilasilan durum bu ve tek basina
           hata numarasi ne yapilacagini anlatmiyor: genelde onceki sunucu
           penceresi hala aciktir ve portu tutuyordur. */
        if (WSAEADDRINUSE == error) {
            printf("  %d portu zaten kullanimda.\n", port);
            printf("  Onceden calisan bir InvoiceServer olabilir; onu kapatin\n");
            printf("  ya da --port <baska_port> ile baslatin.\n");
        }

        closesocket(listenSocket);
        WSACleanup();
        return 1;
    }

    if (SOCKET_ERROR == listen(listenSocket, SOMAXCONN)) {
        printf("Listen basarisiz: %d\n", WSAGetLastError());
        closesocket(listenSocket);
        WSACleanup();
        return 1;
    }

    print_listen_address(port);

    for (;;) {
        struct sockaddr_in clientAddr;
        int clientAddrLength = sizeof(clientAddr);
        SOCKET clientSocket = accept(listenSocket,
                                     (struct sockaddr *)&clientAddr,
                                     &clientAddrLength);

        if (INVALID_SOCKET == clientSocket) {
            printf("Accept basarisiz: %d\n", WSAGetLastError());
            continue;
        }

        printf("Yeni baglanti: %s\n\n", inet_ntoa(clientAddr.sin_addr));

        /* Client ayni baglantiyi hem Hello hem Invoice icin kullaniyor,
           o yuzden baglanti kapanana kadar mesaj almaya devam ediyoruz. */
        serve_client(clientSocket);

        printf("Baglanti kapandi\n\n");
        closesocket(clientSocket);
    }

    /* Buraya normal akista ulasilmiyor (sonsuz accept dongusu); temizlik
       kodu yine de dogru olsun diye duruyor. */
    closesocket(listenSocket);
    xml_shutdown();
    WSACleanup();
    return 0;
}
