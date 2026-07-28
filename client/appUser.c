#include "appUser.h"
#include "appConsole.h"
#include "sqlite3.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <windows.h>
#include <conio.h>

#define DB_FILENAME "upload_system.db"

/* Isterlerde belirtilen bekleme suresi: "Login Succeeded" ve "Login failed"
   mesajlari ekranda tam 2 saniye kalmali. Tek yerde tanimli ki ikisi de
   ayni degeri kullansin. */
#define LOGIN_MESSAGE_MS 2000

/* Program boyunca kullanilan tek veritabani baglantisi.
   "static" oldugu icin sadece bu dosyadan gorulebiliyor: baska bir .c
   dosyasi yanlislikla g_db'ye dokunamaz, veritabani erisimi appUser.c'nin
   sorumlulugunda kalir. C'de dosya duzeyindeki degiskenler varsayilan
   olarak tum projeye acik oldugu icin bu "static" onemli. */
static sqlite3 *g_db = NULL;

/* Login sirasinda audit tablosuna eklenen satirin OID'sini burada
   sakliyoruz ki logout olunca hangi satiri guncelleyecegimizi bilelim. */
static sqlite3_int64 g_currentAuditId = -1;

/* Asagida tanimli; konsol kapatma handler'i da bunu cagirdigi icin
   burada onceden bildiriyoruz. */
static void write_logout_time(void);

/* "users" ve "audit" tablolarini yoksa olusturur. "IF NOT EXISTS" sayesinde
   tablo zaten varsa hata vermeden gecer, tekrar cagirmak guvenlidir.
   Sema, referans upload_system.db dosyasindaki gercek semayla birebir ayni
   tutuluyor (Id/OID INTEGER, tablo adlari kucuk harf). */
static void create_schema(void)
{
    const char *sql =
        "CREATE TABLE IF NOT EXISTS users ("
        "  Id INTEGER NOT NULL PRIMARY KEY,"
        "  NameSurname TEXT,"
        "  Password TEXT"
        ");"
        "CREATE TABLE IF NOT EXISTS audit ("
        "  OID INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  UserId INTEGER,"
        "  LoginTime TEXT,"
        "  LogoutTime TEXT"
        ");";

    char *errorMessage = NULL;

    /* Yoda kosulu: sabit solda. Yanlislikla "=" yazilsa (SQLITE_OK = ...)
       derleyici hata verir; tersi yazimda (x = SQLITE_OK) sessizce atama
       yapilir ve kosul her zaman dogru olurdu. */
    if (SQLITE_OK != sqlite3_exec(g_db, sql, NULL, NULL, &errorMessage)) {
        printf("Tablo olusturma hatasi: %s\n",
               NULL != errorMessage ? errorMessage : "bilinmiyor");
        sqlite3_free(errorMessage);
    }
}

/* Kullanici "2 - Logout" secmeden pencereyi kapatirsa (X tusu, Ctrl+C,
   oturum kapatma) LogoutTime bos kalirdi. Windows bu durumlarda asagidaki
   handler'i cagiriyor; biz de kapanmadan once cikis zamanini kaydediyoruz. */
static BOOL WINAPI console_close_handler(DWORD ctrlType)
{
    (void)ctrlType; /* tum kapanma turlerinde ayni sey yapiliyor */
    write_logout_time();
    return FALSE;   /* FALSE = varsayilan davranis devam etsin, program kapansin */
}

/* Veritabani baglantisi ilk defa mi lazim oluyor diye bakar, oyleyse acar.
   Kalici veri (kullanicilar) ayrica saglanan upload_system.db dosyasindan
   geliyor; burada sadece tablo yoksa olusturuluyor, sahte kullanici
   eklenmiyor. */
static void ensure_db_open(void)
{
    if (NULL != g_db) {
        return;
    }

    if (SQLITE_OK != sqlite3_open(DB_FILENAME, &g_db)) {
        printf("Veritabani acilamadi (%s): %s\n",
               DB_FILENAME, sqlite3_errmsg(g_db));
        exit(1);
    }

    create_schema();

    /* Pencere kapatilirsa da LogoutTime yazilabilsin diye kaydediyoruz */
    SetConsoleCtrlHandler(console_close_handler, TRUE);
}

/* Klavyeden bir satir okur (Enter'a kadar), sonundaki '\n' karakterini siler.
   1 = satir okundu, 0 = girdi bitti (EOF).
   EOF'u ayirt etmek onemli: yonlendirilmis girdiyle calisirken (otomatik
   test) bunu bilmezsek bos kullanici adiyla sonsuz "Login failed" dongusune
   gireriz. */
static int read_line(char *buffer, size_t size)
{
    size_t length;

    if (NULL == fgets(buffer, (int)size, stdin)) {
        buffer[0] = '\0';
        return 0;
    }

    length = strlen(buffer);
    if (0 < length && '\n' == buffer[length - 1]) {
        buffer[length - 1] = '\0';
    }

    return 1;
}

/* Suanki tarih/saati "YYYY-MM-DD HH:MM" formatinda metne cevirir.
   audit.LoginTime / LogoutTime sutunlari TEXT; veritabanindaki mevcut
   kayitlar da bu formatta oldugu icin (ornek: "2021-08-26 08:05")
   saniye kullanmiyoruz. */
static void get_current_timestamp(char *buffer, size_t size)
{
    time_t now = time(NULL);
    struct tm *parts = localtime(&now);

    strftime(buffer, size, "%Y-%m-%d %H:%M", parts);
}

/* Metni hedef tampona guvenle kopyalar: en fazla size-1 karakter yazar ve
   sonuna mutlaka '\0' koyar.

   Neden ayri bir fonksiyon: strncpy tek basina yeterli degil. En fazla n
   karakter kopyalayarak tasmayi onluyor ama kaynak n karakterden uzunsa
   sonuna '\0' KOYMUYOR; boyle bir tamponu sonradan printf/strlen ile
   kullanmak tampon disina tasar. strcpy'nin ise hic sinir kontrolu yok.
   Ikisinin arasindaki bu farki her cagri yerinde tekrar yazmak yerine
   burada tek yerde hallediyoruz. */
static void copy_text(char *destination, size_t size, const char *source)
{
    if (NULL == destination || 0 == size) {
        return;
    }

    if (NULL == source) {
        destination[0] = '\0';
        return;
    }

    strncpy(destination, source, size - 1);
    destination[size - 1] = '\0';
}

/* "users" tablosunda Id'si eslesen bir satir var mi, sifre dogru mu diye bakar.
   Bulursa NameSurname degerini nameSurnameOut'a yazar ve 1 doner. */
static int check_credentials(const char *id, const char *password,
                             char *nameSurnameOut, size_t nameSize)
{
    const char *sql = "SELECT NameSurname, Password FROM users WHERE Id = ?;";
    sqlite3_stmt *stmt;
    int found = 0;

    /* Kullanici girdisi sorguya metin olarak yapistirilmiyor, parametre
       olarak baglaniyor (sqlite3_bind_text). Boylece girilen Id ne olursa
       olsun SQL olarak yorumlanmiyor. */
    if (SQLITE_OK != sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL)) {
        return 0;
    }

    sqlite3_bind_text(stmt, 1, id, -1, SQLITE_TRANSIENT);

    if (SQLITE_ROW == sqlite3_step(stmt)) {
        const unsigned char *dbPassword = sqlite3_column_text(stmt, 1);

        if (NULL != dbPassword &&
            0 == strcmp((const char *)dbPassword, password)) {
            copy_text(nameSurnameOut, nameSize,
                      (const char *)sqlite3_column_text(stmt, 0));
            found = 1;
        }
    }

    sqlite3_finalize(stmt);
    return found;
}

/* Basarili giriste audit tablosuna yeni bir satir ekler, OID'sini
   g_currentAuditId'ye kaydeder. */
static void insert_login_audit(const char *userId)
{
    const char *sql = "INSERT INTO audit (UserId, LoginTime) VALUES (?, ?);";
    sqlite3_stmt *stmt;
    char timestamp[32];

    get_current_timestamp(timestamp, sizeof(timestamp));

    if (SQLITE_OK == sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL)) {
        sqlite3_bind_text(stmt, 1, userId, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, timestamp, -1, SQLITE_TRANSIENT);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);

        g_currentAuditId = sqlite3_last_insert_rowid(g_db);
    }
}

int user_login(AppUser *outUser)
{
    char id[64];
    char password[64];
    char nameSurname[128];

    ensure_db_open();

    for (;;) {
        printf("Enter User Id: ");
        if (0 == read_line(id, sizeof(id))) {
            return 0; /* girdi bitti */
        }

        printf("Enter Password: ");
        console_read_password(password, sizeof(password));

        if (0 != check_credentials(id, password, nameSurname, sizeof(nameSurname))) {
            printf("Login Succeeded\n");
            Sleep(LOGIN_MESSAGE_MS);

            copy_text(outUser->id, sizeof(outUser->id), id);
            copy_text(outUser->nameSurname, sizeof(outUser->nameSurname), nameSurname);
            copy_text(outUser->password, sizeof(outUser->password), password);

            insert_login_audit(outUser->id);
            return 1;
        }

        printf("Login failed\n");
        Sleep(LOGIN_MESSAGE_MS);
        /* dongu basa doner -> tekrar "Enter User Id" sorulur (adim 1.4.b) */
    }
}

/* Acik olan audit kaydinin LogoutTime sutununu su anki zamanla doldurur.
   Hem normal logout hem de pencere kapatma durumunda cagriliyor. */
static void write_logout_time(void)
{
    const char *sql = "UPDATE audit SET LogoutTime = ? WHERE OID = ?;";
    sqlite3_stmt *stmt;
    char timestamp[32];

    if (0 > g_currentAuditId) {
        return; /* acik bir oturum yok */
    }

    get_current_timestamp(timestamp, sizeof(timestamp));

    if (SQLITE_OK == sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL)) {
        sqlite3_bind_text(stmt, 1, timestamp, -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(stmt, 2, g_currentAuditId);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }

    g_currentAuditId = -1;
}

void user_logout(const AppUser *user)
{
    (void)user; /* su an kullanilmiyor ama ileride loglama icin lazim olabilir */

    write_logout_time();
    Sleep(2000);
}
