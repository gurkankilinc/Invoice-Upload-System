#include "appNetwork.h"
#include "appProto.h"
#include "appConsole.h"

#include <libxml/parser.h>

#include <winsock2.h>
#include <ws2tcpip.h>

#include <direct.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Sunucudan gelen imzali XML dosyalarinin yazildigi klasor.
   Calisma dizini client/ oldugu icin bir ust seviyedeki signed/ klasoru.
   Uretilen tum XML ciktilari tek yerde toplansin diye kaynak dosyalarin
   yanina degil ayri klasore yaziyoruz. */
#define SIGNED_DIR "../signed"

/* Yanit beklerken kabul edilen en uzun sure. Spesifikasyon 30 saniye diyor. */
#define RECEIVE_TIMEOUT_MS 30000

/* Baglanti koptugunda ne kadar sure boyunca yeniden denenecek ve denemeler
   arasinda ne kadar beklenecek.

   Deneme SAYISI yerine SURE sinirlamamizin sebebi: basarisiz bir connect()
   cagrisi ne kadar surecegi onceden belli degil. Kapali bir porta aninda
   "reddedildi" doner, ama erisilemeyen bir makineye TCP yeniden denemeleri
   yuzunden 20 saniyeye kadar bloklanabilir. Sayiya baglasaydik toplam sure
   duruma gore 1 saniye de olurdu 60 saniye de; sureye baglayinca her
   durumda yaklasik 30 saniye deniyoruz. */
#define RECONNECT_WINDOW_MS 30000
#define RECONNECT_WAIT_MS    2000

static SOCKET g_serverSocket = INVALID_SOCKET;

/* Baglanti bilgileri, yeniden baglanabilmek icin saklaniyor: kullaniciya
   her kopmada tekrar IP/port sormak yerine bildigimiz adrese doniyoruz. */
static char g_serverIp[64] = "";
static int  g_serverPort = 0;

/* Yeniden baglandiktan sonra Hello mesajini tekrar gonderebilmek icin
   giris bilgileri. Sunucu her yeni baglantiyi yeni bir oturum olarak
   gordugu icin kimligimizi tekrar bildirmemiz gerekiyor. */
static char g_userName[128] = "";
static char g_userPassword[64] = "";

/* Son baglanma denemesinin hata kodu. Ayri saklamamizin sebebi:
   closesocket() basarili oldugunda WSAGetLastError() degerini sifirliyor,
   yani soketi kapattiktan sonra sorarsak sebebi kaybediyoruz. */
static int g_lastConnectError = 0;

/* Asagida tanimli; sendAndReceive baglanti koptugunda bunu cagiriyor. */
static int tryReconnect(void);

/* Klavyeden bir satir okur, sonundaki satir sonu karakterlerini temizler. */
static void readLine(char *buffer, size_t size)
{
    size_t length;

    if (NULL == fgets(buffer, (int)size, stdin)) {
        buffer[0] = '\0';
        return;
    }

    length = strlen(buffer);
    while (0 < length && ('\n' == buffer[length - 1] || '\r' == buffer[length - 1])) {
        buffer[length - 1] = '\0';
        length--;
    }
}

/* Mesaji gonderir, yaniti bekler.
   sentFileName NULL degilse, gonderim ile yanit beklemesi ARASINDA
   "... file has been sent, waiting for registration" bilgilendirmesi
   yazdirilir (isterler listesi bunu istemci ekraninda istiyor).
   Basarili olursa 1 doner ve *responseOut'a protoFree ile birakilacak
   tampon yazar. Yanit gelmezse (30 sn zaman asimi dahil) 0 doner. */
static int sendAndReceive(const char *message, size_t messageLength,
                            const char *sentFileName,
                            char **responseOut, size_t *responseLengthOut)
{
    int result;

    *responseOut = NULL;
    *responseLengthOut = 0;

    if (INVALID_SOCKET == g_serverSocket) {
        printf("Upload operation failed\n");
        printf("  Sebep: sunucu baglantisi yok\n");
        tryReconnect();
        return 0;
    }

    result = protoSend(g_serverSocket, message, messageLength);
    if (PROTO_OK != result) {
        printf("Upload operation failed\n");
        printf("  Sebep: mesaj gonderilemedi (baglanti kopmus)\n");
        tryReconnect();
        return 0;
    }

    if (NULL != sentFileName) {
        char utf8Name[512];
        printf("\n%s file has been sent, waiting for registration\n",
               consoleToUtf8(sentFileName, utf8Name, sizeof(utf8Name)));
    }

    result = protoRecv(g_serverSocket, responseOut, responseLengthOut);
    if (PROTO_OK != result) {
        /* Spesifikasyonun istedigi mesaj; sebebi ayrica yaziyoruz ki
           kullanici "sunucu yavas mi, kapali mi" ayrimini gorebilsin. */
        printf("Upload operation failed\n");

        if (PROTO_TIMEOUT == result) {
            printf("  Sebep: sunucudan %d saniye icinde yanit gelmedi\n",
                   RECEIVE_TIMEOUT_MS / 1000);
            printf("  Baglanti sifirlaniyor (gec gelen yanit bir sonraki\n");
            printf("  faturanin imzasi sanilmasin diye)\n");
        } else if (PROTO_CLOSED == result) {
            printf("  Sebep: sunucu baglantiyi kapatti\n");
        } else {
            printf("  Sebep: ag hatasi (%d)\n", WSAGetLastError());
        }

        tryReconnect();
        return 0;
    }

    return 1;
}

/* Sunucudan gelen imzali XML'i signed/ klasorune kaydeder.
   "Hakan Uslu.inv" -> "../signed/Hakan Uslu.signed.xml"
   Uzanti bilerek ".inv" degil: yoksa bu dosyalar da fatura listesine duserdi. */
static void saveSignedFile(const char *fileName,
                             const char *signedContent, size_t contentLength)
{
    char outPath[512];
    size_t baseLength = strlen(fileName);
    FILE *fp;

    /* Klasor yoksa olustur. Zaten varsa _mkdir hata doner, sorun degil. */
    _mkdir(SIGNED_DIR);

    /* ".inv" uzantisini at, yerine ".signed.xml" koy */
    if (4 < baseLength) {
        baseLength -= 4;
    }

    /* snprintf, sprintf'ten farkli olarak hedef tamponun boyutunu biliyor ve
       tasma olacaksa yazmayi kesiyor. Dosya adi dizinden geldigi icin
       uzunlugu bizim kontrolumuzde degil, bu yuzden onemli. */
    snprintf(outPath, sizeof(outPath), "%s/%.*s.signed.xml",
             SIGNED_DIR, (int)baseLength, fileName);

    /* "wb": XML'i sunucudan geldigi gibi, bayt bayt yaziyoruz. "w" olsaydi
       Windows her '\n' karakterini "\r\n" yapardi ve dosyanin hash'i
       sunucunun hesapladigindan farkli cikardi. */
    fp = fopen(outPath, "wb");
    if (NULL == fp) {
        printf("Imzali dosya kaydedilemedi: %s\n", outPath);
        return;
    }

    fwrite(signedContent, 1, contentLength, fp);
    fclose(fp);

    printf("\nImzali dosya kaydedildi: %s\n", outPath);
}

int networkSendHello(const char *nameSurname, const char *password)
{
    char message[512];
    char *response = NULL;
    size_t responseLength = 0;
    int received;

    /* Bilgileri sakliyoruz: baglanti koparsa yeniden baglandiktan sonra
       Hello'yu tekrar gondermemiz gerekiyor (bkz. resendHelloQuietly). */
    snprintf(g_userName, sizeof(g_userName), "%s", nameSurname);
    snprintf(g_userPassword, sizeof(g_userPassword), "%s", password);

    snprintf(message, sizeof(message), "<Hello>%s %s</Hello>",
             nameSurname, password);

    received = sendAndReceive(message, strlen(message), NULL,
                                &response, &responseLength);
    if (0 != received) {
        /* Spesifikasyon: "InvoiceClient display message from the Server" */
        printf("%s\n", response);
    }

    protoFree(response);
    return received;
}

int networkSendInvoice(const char *fileName,
                         const char *fileContent, size_t contentLength)
{
    char header[320];
    char *message;
    size_t headerLength;
    size_t messageLength;
    const char *closeTag = "</Invoice>";
    char *response = NULL;
    size_t responseLength = 0;
    int received;
    xmlDocPtr doc;

    snprintf(header, sizeof(header), "<Invoice name=\"%s\">", fileName);
    headerLength = strlen(header);
    messageLength = headerLength + contentLength + strlen(closeTag);

    /* Fatura icerigi sabit boyutlu bir diziye sigmayabilir; mesaji tam
       olcusunde ayirip parcalari sirayla kopyaliyoruz. */
    message = (char *)malloc(messageLength + 1);
    if (NULL == message) {
        printf("Bellek ayrilamadi\n");
        return 0;
    }

    memcpy(message, header, headerLength);
    memcpy(message + headerLength, fileContent, contentLength);
    memcpy(message + headerLength + contentLength, closeTag, strlen(closeTag));
    message[messageLength] = '\0';

    received = sendAndReceive(message, messageLength, fileName,
                                &response, &responseLength);
    free(message);

    if (0 == received) {
        return 0;
    }

    /* Spesifikasyon: "InvoiceClient prints data in the response to the screen" */
    printf("\n%s\n", response);

    /* Yanit gercekten ayristirilabilir bir XML belgesi mi? Sunucu hata
       durumunda duz bir <Response>...</Response> metni donebiliyor; onu
       imzali fatura diye kaydetmeyelim. */
    doc = xmlReadMemory(response, (int)responseLength, "response.xml", NULL,
                        XML_PARSE_NOERROR | XML_PARSE_NOWARNING);

    if (NULL == doc) {
        printf("\nUyari: sunucudan gelen yanit gecerli bir XML belgesi degil, "
               "dosya kaydedilmedi.\n");
    } else {
        xmlFreeDoc(doc);
        saveSignedFile(fileName, response, responseLength);
    }

    protoFree(response);
    return 1;
}

/* Saklanan adrese TCP baglantisi acar. Sessizdir; mesaji cagiran yazar.
   1 = baglandi, 0 = baglanamadi (sebep g_lastConnectError'da). */
static int openConnection(void)
{
    struct sockaddr_in serverAddr;
    DWORD timeout = RECEIVE_TIMEOUT_MS;

    g_lastConnectError = 0;

    g_serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (INVALID_SOCKET == g_serverSocket) {
        g_lastConnectError = WSAGetLastError();
        return 0;
    }

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons((u_short)g_serverPort);
    serverAddr.sin_addr.s_addr = inet_addr(g_serverIp);

    if (SOCKET_ERROR == connect(g_serverSocket, (struct sockaddr *)&serverAddr,
                                sizeof(serverAddr))) {
        /* Hata kodunu HEMEN aliyoruz: closesocket() basarili oldugunda
           WSAGetLastError() degerini sifirliyor ve sebebi kaybediyorduk. */
        g_lastConnectError = WSAGetLastError();
        closesocket(g_serverSocket);
        g_serverSocket = INVALID_SOCKET;
        return 0;
    }

    /* Spesifikasyon: 30 saniye icinde yanit gelmezse hata mesaji.
       SO_RCVTIMEO sayesinde recv() bu sureden sonra kendiliginden hata
       donuyor, ayrica bir zamanlayici kurmaya gerek kalmiyor. */
    setsockopt(g_serverSocket, SOL_SOCKET, SO_RCVTIMEO,
               (const char *)&timeout, sizeof(timeout));

    return 1;
}

/* Yeniden baglandiktan sonra sunucuya kimligimizi tekrar bildirir.
   Yeni baglanti sunucu icin yeni bir oturum; Hello gonderilmezse sunucu
   ekraninda kullanici adi gorunmez. Yanit okunur ama ekrana basilmaz,
   kullaniciyi ikinci bir "I am ready" ile mesgul etmeyelim. */
static void resendHelloQuietly(void)
{
    char message[512];
    char *response = NULL;
    size_t responseLength = 0;

    if ('\0' == g_userName[0]) {
        return; /* henuz hic Hello gonderilmemis */
    }

    snprintf(message, sizeof(message), "<Hello>%s %s</Hello>",
             g_userName, g_userPassword);

    if (PROTO_OK == protoSend(g_serverSocket, message, strlen(message))) {
        protoRecv(g_serverSocket, &response, &responseLength);
        protoFree(response);
    }
}

/* Baglantiyi kapatip saklanan adrese yeniden baglanmayi dener.
   Her adimi ekrana yazar ki kullanici ne olup bittigini gorsun.
   1 = yeniden baglanildi, 0 = baglanilamadi. */
static int tryReconnect(void)
{
    DWORD startTick;
    DWORD elapsed;
    int attempt = 0;

    /* Eski soketi mutlaka kapatiyoruz. Zaman asiminda soket teknik olarak
       hala acik, ama kullanmaya devam etmek TEHLIKELI: sunucu geciken yaniti
       sonradan gonderirse, o veri bir sonraki faturanin yaniti saniliyor.
       Baglantiyi sifirlamak bu karisikligi tamamen ortadan kaldiriyor. */
    if (INVALID_SOCKET != g_serverSocket) {
        closesocket(g_serverSocket);
        g_serverSocket = INVALID_SOCKET;
    }

    printf("  Yeniden baglanmaya calisiliyor (en fazla %lu saniye)...\n",
           (unsigned long)(RECONNECT_WINDOW_MS / 1000));

    startTick = GetTickCount();

    for (;;) {
        attempt++;

        /* Cikarma islemi unsigned oldugu icin, GetTickCount 49 gunde bir
           basa donse bile gecen sure dogru hesaplanir. */
        elapsed = GetTickCount() - startTick;

        printf("  %2lu. saniye - deneme %d: %s:%d ... ",
               (unsigned long)(elapsed / 1000), attempt,
               g_serverIp, g_serverPort);

        if (0 != openConnection()) {
            printf("basarili\n");
            resendHelloQuietly();
            printf("  Sunucuya yeniden baglanildi, islemi tekrar deneyebilirsiniz.\n\n");
            return 1;
        }

        printf("basarisiz (hata %d)\n", g_lastConnectError);

        /* Denemenin kendisi de zaman aldigi icin sureyi yeniden olcuyoruz */
        elapsed = GetTickCount() - startTick;
        if (elapsed >= RECONNECT_WINDOW_MS) {
            break;
        }

        /* Kalan sureden fazla beklemeyelim ki pencere 30 saniyeyi asmasin */
        {
            DWORD remaining = RECONNECT_WINDOW_MS - elapsed;
            Sleep(remaining < RECONNECT_WAIT_MS ? remaining : RECONNECT_WAIT_MS);
        }
    }

    printf("  %lu saniye boyunca sunucuya ulasilamadi.\n",
           (unsigned long)(elapsed / 1000));
    printf("  Sunucunun calistigini kontrol edip islemi tekrar deneyin\n");
    printf("  (menu -> Invoice List).\n\n");
    return 0;
}

int networkConnect(void)
{
    WSADATA wsaData;
    char portText[16];

    if (0 != WSAStartup(MAKEWORD(2, 2), &wsaData)) {
        printf("WSAStartup basarisiz\n");
        return 0;
    }

    /* Spesifikasyon: login'den sonra ip ve port kullaniciya sorulacak.
       Degerleri sakliyoruz ki baglanti koptugunda tekrar sormadan
       yeniden baglanabilelim. */
    printf("Enter Server IP: ");
    readLine(g_serverIp, sizeof(g_serverIp));

    printf("Enter Server Port: ");
    readLine(portText, sizeof(portText));
    g_serverPort = atoi(portText);

    if (0 == openConnection()) {
        printf("Sunucuya baglanilamadi (hata %d)\n", g_lastConnectError);
        return 0;
    }

    printf("Sunucuya baglanildi.\n");
    return 1;
}

void networkDisconnect(void)
{
    if (INVALID_SOCKET != g_serverSocket) {
        closesocket(g_serverSocket);
        g_serverSocket = INVALID_SOCKET;
    }

    xmlCleanupParser();
    WSACleanup();
}
