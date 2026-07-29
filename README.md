# Invoice Upload System

Spesifikasyonlar (masaüstünde):
`Specifications for Invoice Upload System - Phase 1.pdf` ve `... Phase 2.pdf`.

İki ayrı C konsol uygulaması:

- **InvoiceClient** — SQLite üzerinden login, `.inv` fatura dosyalarını listeleme,
  seçilen faturayı TCP ile sunucuya gönderme, imzalı XML'i kaydetme.
- **InvoiceServer** — gelen faturanın SHA-256 hash'ini alır, hash'i AES-256 ile
  şifreler, faturaya `<invoiceStatus>` node'unu ekleyip imzalı XML'i geri döner.

Spesifikasyon ikisinin **ayrı makinelerde** çalışmasını istiyor; bu yüzden client
ve server birbirinin dosyalarına değil, sadece TCP protokolüne bağlı.

## Klasör yapısı

| Klasör / dosya | İçerik |
|---|---|
| `client/` | InvoiceClient kaynakları, `.inv` faturalar, `upload_system.db` |
| `server/` | InvoiceServer kaynakları (ağ, imzalama, XML) |
| `common/` | Client ve server'ın **ortak** kullandığı mesaj çerçeveleme katmanı |
| `signed/` | Sunucudan dönen imzalı XML dosyaları buraya yazılır |
| `third_party/` | sqlite3, mbedTLS, libxml2 (git'e dahil değil) |
| `tests/` | Otomatik sunucu testleri |

Derlenen `.exe` dosyaları kendi klasörlerine yazılır
(`client/InvoiceClient.exe`, `server/InvoiceServer.exe`), yani her klasör
kendi başına taşınabilir/zip'lenebilir. `.gitignore` sayesinde git'e girmezler.
Kökteki `InvoiceClient.bat` / `InvoiceServer.bat` sadece kısayol; ilgili
klasördeki exe'yi çağırırlar.

### Client dosyaları

| Dosya | Sorumluluk |
|---|---|
| `appMain.c` | `main()`, "Select Operation" menüsü |
| `appUser.c` / `.h` | Login / Logout, sqlite bağlantısı, `audit` tablosu |
| `appInvoice.c` / `.h` | `.inv` listeleme, dosyayı okuyup sunucuya gönderme |
| `appNetwork.c` / `.h` | TCP bağlantısı, Hello / Invoice mesajları, imzalı dosyayı kaydetme |

Spesifikasyonun zorunlu tuttuğu beş dosya (`appMain.c`, `appUser.c/h`,
`appInvoice.c/h`) burada; `appNetwork.c/h` Phase 2 ile eklendi.

### Server dosyaları

| Dosya | Sorumluluk |
|---|---|
| `appServer.c` | `main()`, TCP dinleme, mesaj tipine göre yönlendirme (Step 3.1) |
| `appSign.c` / `.h` | SHA-256 hash (Step 3.2) ve AES-256 şifreleme (Step 3.3) — mbedTLS |
| `appXml.c` / `.h` | `<invoiceStatus>` node'unun faturaya eklenmesi — libxml2 |

## Kullanılan kütüphaneler

| Kütüphane | Nerede | Ne için |
|---|---|---|
| **sqlite3** | client | `users` / `audit` tabloları (Phase 1 zorunlu tutuyor) |
| **mbedTLS** | server | SHA-256 ve AES-256 |
| **libxml2** | server + client | XML ayrıştırma, node ekleme, doğrulama |

mbedTLS ve libxml2 elle yazılmış kripto/XML kodunun yerini aldı. Önceki sürümde
hash ve AES Windows'a özel `wincrypt` API'siyle yapılıyordu, XML node'u ise metin
birleştirmeyle ekleniyordu.

Üçü de `third_party/` altında kaynak olarak duruyor ve projeyle birlikte
derleniyor — sistemde kurulu bir pakete ihtiyaç yok. `.gitignore`'da oldukları
için git'e girmezler.

**mbedTLS** varsayılan yapılandırması yerine `third_party/mbedtls_config_invoice.h`
kullanılıyor: sadece SHA-256 ve AES açık, geri kalan her şey kapalı. Böylece
kütüphanenin tamamı yerine 5 kaynak dosya derlemek yetiyor.
`third_party/mbedtls_platform_mingw.c`, MinGW.org 6.3'te bulunmayan
`SecureZeroMemory`'nin yerini tutuyor (mbedTLS kaynağına hiç dokunulmadı).

**libxml2** `win32/Makefile.mingw` ile statik olarak derlendi; iconv, zlib ve
lzma kapalı, yani harici bağımlılığı yok.

## Derleme

Proje kökünden:

```bash
sh build.sh
```

Sadece bir tarafı derlemek için `sh build.sh client` veya `sh build.sh server`.

libxml2 bir kereye mahsus ayrıca derlenmeli (zaten derlenmiş olarak duruyor):

```bash
cd third_party/libxml2/win32 && cscript //Nologo configure.js compiler=mingw iconv=no icu=no zlib=no lzma=no ftp=no http=no html=no python=no modules=no schemas=no schematron=no c14n=no catalog=no xinclude=no xptr=no threads=no mem_debug=no xml_debug=no && mingw32-make -f Makefile.mingw libxmla
```

## Çalıştırma

Proje kökünden (VS Code terminali de burada açılır), iki ayrı pencerede:

```bash
.\InvoiceServer
```

```bash
.\InvoiceClient
```

Bunlar kökteki `InvoiceServer.bat` / `InvoiceClient.bat` başlatıcılarıdır; asıl
`.exe` dosyaları `server\` ve `client\` içinde durur. Doğrudan da
çalıştırabilirsiniz (`.\server\InvoiceServer.exe`). Parametreler aynen geçer:
`.\InvoiceServer --delay 35`

Hangi klasörden çalıştırıldıkları önemli değil: client açılışta çalışma dizinini
kendi `.exe` konumuna sabitliyor (`setWorkingDirectoryToExe()`), böylece
`.inv` dosyalarını ve `upload_system.db`'yi her zaman buluyor. Çift tıklayarak
da çalıştırılabilirler.

Sunucu açılışta dinlediği IP ve portu yazar. Port varsayılan 54000;
`--port <n>` ile değiştirilebilir. Port meşgulse bunu açıkça söyler —
genelde sebebi önceki sunucu penceresinin hâlâ açık olmasıdır.

Client'ta login'den sonra sunucunun IP ve portu sorulur.

### Faz 1 modu

İsterler listesi Faz 1'in "içeriği 10 saniye ekranda göster" davranışını da
ayrı bir madde olarak sayıyor. Faz 2 bu davranışın yerine dosyayı sunucuya
göndermeyi getirdi; ikisi de tek sürümden gösterilebilsin diye Faz 1
davranışı bir seçenek olarak duruyor:

```bash
.\InvoiceClient --phase1
```

Bu modda sunucuya hiç bağlanılmaz (IP/port sorulmaz), seçilen faturanın
içeriği ekrana basılır ve tam 10 saniye kalır.

### Test kullanıcıları

`client/upload_system.db` içindeki gerçek kayıtlar:

| Id | NameSurname | Password |
|---|---|---|
| 1 | MERT GEZER | 1ABC3 |
| 2 | UMUT KARAKAYA | CD1ABC3 |
| 3 | LALENUR ALTINOZ | KE3733 |

`client/seed.sql`, veritabanı silinirse aynı verileri yeniden eklemek için
referans olarak duruyor (`sqlite3 upload_system.db < seed.sql`). Uygulama
kendiliğinden sahte kullanıcı üretmiyor, sadece tablo yoksa oluşturuyor.

## Testler

İki test dosyası var; ikisi de proje kökünden çalıştırılır.

```bash
python tests/test_client.py
```

İstemciyi gerçekten çalıştırıp çıktısını **zaman damgasıyla** okur ve
isterlerdeki süreleri ölçer (2 sn / 10 sn / 30 sn). Login, fatura listesi,
logout, audit tablosu, fatura yükleme ve zaman aşımı senaryolarını kapsar.

```bash
python tests/test_server.py --slow
```

Sunucuyu başlatıp kendi TCP istemcisi gibi davranarak spesifikasyon maddelerini
doğrular. Hash ve imzayı Python'un bağımsız `hashlib` / `cryptography`
kütüphaneleriyle yeniden hesaplayıp karşılaştırır — yani sunucu kendi kendini
doğrulamıyor. `--slow` olmadan 30 saniyelik zaman aşımı testi atlanır.

```bash
python tests/test_reconnect.py
```

Sunucu kapandığında / yanıt gecikince istemcinin ne yaptığını doğrular:
hata sebebini ayırt ediyor mu, yeniden bağlanmayı deniyor mu, sunucu geri
gelince toparlanıyor mu.

**İstemci nasıl otomatik test edilebiliyor:** menüler normalde `_getch()` ile
tek tuş okur, yani borudan beslenemez. `common/appConsole.c` bunu çözüyor —
`stdin` bir boruya yönlendirilmişse tek tuş yerine satırdan okuyor. Uygulamanın
davranışı değişmiyor, sadece girdinin geldiği yer değişiyor.

## Protokol notu

Mesajlar telde spesifikasyondaki gibi gidiyor: `<Hello>ad soyad şifre</Hello>`,
`<Invoice name="...">...</Invoice>`, `<Response>I am ready</Response>`.

Bunların **önüne 4 baytlık uzunluk bilgisi** ekleniyor (`common/appProto.c`).
Sebebi: TCP bir mesaj protokolü değil, bayt akışı protokolü. Tek `send()` ile
gönderilen 900 baytlık XML karşı tarafa iki ayrı `recv()`'de parça parça
gelebilir. Önceki kod "bir recv = bir mesaj" varsayıyordu ve fatura dosyaları
küçük olduğu için tesadüfen çalışıyordu; XML faturalarla birlikte bu varsayım
kırılacaktı. Uzunluk öneki sayesinde alıcı kaç bayt bekleyeceğini biliyor.

Mesajın **içeriği** değişmedi, sadece önünde uzunluğu duruyor.

## Bağlantı kurtarma

Yanıt alınamadığında istemci yalnızca "Upload operation failed" demekle
kalmıyor; sebebi ayırt edip saklanan adrese yeniden bağlanmayı deniyor:

```
Upload operation failed
  Sebep: sunucudan 30 saniye icinde yanit gelmedi
  Baglanti sifirlaniyor (gec gelen yanit bir sonraki
  faturanin imzasi sanilmasin diye)
  Yeniden baglanmaya calisiliyor (en fazla 30 saniye)...
   0. saniye - deneme 1: 192.168.50.53:54000 ... basarisiz (hata 10061)
   4. saniye - deneme 2: 192.168.50.53:54000 ... basarili
  Sunucuya yeniden baglanildi, islemi tekrar deneyebilirsiniz.
```

Deneme **sayısı** yerine **süre** sınırlanıyor (30 saniye): başarısız bir
`connect()` çağrısının ne kadar süreceği önceden belli değil. Kapalı bir
porta anında "reddedildi" döner, ama erişilemeyen bir makineye TCP yeniden
denemeleri yüzünden 20 saniyeye kadar bloklanabilir. Sayıya bağlasaydık
toplam süre duruma göre 1 saniye de olurdu 60 saniye de.

Sunucu bu pencere içinde geri gelirse istemci kullanıcıdan hiçbir şey
istemeden kendiliğinden bağlanır.

`common/appProto.c` üç farklı başarısızlığı ayrı kodlarla döndürüyor
(`PROTO_TIMEOUT`, `PROTO_CLOSED`, `PROTO_ERROR`) — eskiden hepsi tek bir
`-1` idi ve kullanıcı "sunucu yavaş mı, kapalı mı" ayrımını göremiyordu.

**Zaman aşımından sonra bağlantının sıfırlanması sadece kolaylık değil,
doğruluk meselesi:** sunucu geciken yanıtı 35. saniyede gönderirse o veri
soketin tamponunda bekler ve bir sonraki fatura gönderiminde **o faturanın
imzası sanılarak** kaydedilirdi. Bağlantıyı kapatmak bu riski tamamen
ortadan kaldırıyor.

Yeniden bağlandıktan sonra `Hello` mesajı otomatik olarak tekrar gönderilir
(sessizce), çünkü sunucu her yeni bağlantıyı yeni bir oturum sayıyor.

## Kod düzeni kuralları

- **Yoda koşulları**: sabit solda (`NULL == fp`, `0 == count`). Yanlışlıkla
  `=` yazılırsa derleyici hata verir; ters yazımda sessizce atama yapılırdı.
- **`static`**: dosya dışından çağrılması gerekmeyen her fonksiyon `static`.
  C'de dosya düzeyindeki isimler varsayılan olarak tüm projeye açıktır.
- **`strncpy` / `copy_text`**: `strcpy`'nin sınır kontrolü yok. `strncpy` en
  fazla n karakter kopyalar ama kaynak uzunsa sonuna `'\0'` **koymaz**;
  `client/appUser.c` içindeki `copyText()` ikisini de halleder.
- **`snprintf`**: `sprintf` yerine her yerde `snprintf` — hedef tamponun
  boyutunu bildiği için taşma olacaksa yazmayı keser.

## Bilinen sınırlamalar

- Şifre girişinde backspace/düzeltme yok.
- Invoice List en fazla 9 dosya gösteriyor (tek tuşla seçim yapılabilsin diye).
- Sunucu aynı anda tek client'a hizmet veriyor (sıralı `accept`). Spesifikasyon
  eşzamanlılık istemiyor.
- Hello mesajı şifreyi de taşıyor — spesifikasyon açıkça böyle istiyor
  (`<Hello>logged user name and password</Hello>`). Sunucu şifreyi hiçbir yere
  yazmıyor, ama trafik şifresiz olduğu için gerçek bir sistemde bu kabul
  edilemez olurdu.
- AES **ECB** modunda kullanılıyor; spesifikasyon sadece anahtarı veriyor, mod
  belirtmiyor. ECB aynı girdi için hep aynı çıktıyı verir (imzanın tekrar
  üretilebilir olması bu yüzden mümkün), ama genel amaçlı şifreleme için
  uygun bir mod değildir.
- Veritabanı bağlantısı program kapanırken elle kapatılmıyor
  (`sqlite3_close` çağrılmıyor) — process sonlanınca işletim sistemi temizler.
