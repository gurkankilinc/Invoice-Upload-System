# Değişiklik Raporu

Bu doküman, projenin Faz 2 teslimine hazırlanması sırasında yapılan
değişiklikleri ve **neden** yapıldıklarını anlatır.

Başlangıç noktası: `35bd515` — proje derlenmiyordu, imzalı çıktı geçersiz XML
üretiyordu, otomatik test yoktu.

Bitiş noktası: `phase2-xml-libraries-and-tests` dalı — **76 otomatik kontrol
geçiyor**, tek elle yapılacak iş iki makine testi kaldı.

---

## 1. Derlemeyi kıran hatalar

Proje devralındığında **hiç derlenmiyordu**. Dört sözdizimi hatası vardı:

| Dosya | Hata | Olması gereken |
|---|---|---|
| `appInvoice.c` | `for (;;  \|\| counter == 0)` | `for (;;)` |
| `appInvoice.c` | tanımsız `counter--` | (silindi) |
| `appInvoice.c` | `if (3 == fp)` | `if (NULL == fp)` |
| `appServer.c` | `char 2Hex[65];` | `char hashHex[65];` |

`if (3 == fp)` özellikle tehlikeliydi: dosya açılamadığında koşul hiçbir zaman
doğru olmayacağı için `NULL` işaretçiyle `fread` çağrılıyor ve program
çöküyordu.

---

## 2. Asıl sorun: geçersiz XML üretimi

Spesifikasyon, sunucunun faturaya şu düğümü eklemesini istiyor:

```xml
<invoiceStatus date="YYYY-MM-DD hh:mm:ss">
    <message>Fatura Kaydedildi</message>
    <signature>...</signature>
</invoiceStatus>
```

Eski kod bunu **metin birleştirmeyle**, gelen içeriğin sonuna ekliyordu:

```c
snprintf(signedXml, sizeof(signedXml), "%s<invoiceStatus date=\"%s\">...",
         content, timestamp, signatureHex);
```

Fatura dosyaları düz metinken bu fark edilmiyordu. Gerçek XML faturalara
geçilince ortaya çıktı: düğüm `</uploadSystem>` kapanışından **sonra**
ekleniyordu. Bir XML belgesinde kök elemanın dışında eleman bulunamaz, yani
üretilen dosya geçersizdi.

**Çözüm:** `server/appXml.c` — libxml2 ile belge ağaç olarak ayrıştırılıyor,
düğüm kökün içine ve kökün namespace'ini devralarak ekleniyor. Ayrıca aynı
fatura tekrar imzalanırsa eski düğüm silinip yenisi yazılıyor.

---

## 3. Üçüncü parti kütüphaneler

Kripto ve XML işlemleri elle yazılmış koddan kütüphanelere taşındı.

| Kütüphane | Nerede | Öncesi |
|---|---|---|
| **mbedTLS** | sunucu | Windows'a özel `wincrypt` API'si |
| **libxml2** | sunucu + istemci | metin birleştirme |

Makinede paket yöneticisi (MSYS2/vcpkg) olmadığı için ikisi de `third_party/`
altına kaynak olarak indirilip projeyle birlikte derleniyor — sqlite3 ile aynı
yaklaşım.

**mbedTLS** varsayılan yapılandırma yerine `third_party/mbedtls_config_invoice.h`
kullanıyor: yalnızca SHA-256 ve AES açık, kütüphanenin tamamı yerine 5 kaynak
dosya derleniyor. MinGW.org 6.3'te `SecureZeroMemory` bulunmadığı için
mbedTLS'in kendi uzantı noktası (`MBEDTLS_PLATFORM_ZEROIZE_ALT`) kullanıldı —
vendor kaynağına hiç dokunulmadı, güncellemesi kolay kaldı.

**libxml2** `win32/Makefile.mingw` ile statik derlendi; iconv, zlib ve lzma
kapalı, harici bağımlılığı yok.

**OpenSSL neden değil:** kendi build sistemi (Perl + nasm) gerektiriyor.
mbedTLS saf C ve MinGW ile sorunsuz derleniyor.

---

## 4. Ağ katmanı: iki gerçek hata

### 4.1 "Bir recv = bir mesaj" varsayımı

Eski kod tek `recv()` çağrısının tüm mesajı getirdiğini varsayıyordu. TCP bir
mesaj protokolü değil, **bayt akışı** protokolü: tek `send()` ile giden 900
baytlık XML karşı tarafa iki ayrı `recv()`'de parça parça gelebilir.

Fatura dosyaları küçük olduğu için bu tesadüfen çalışıyordu; XML faturalarla
birlikte kırılacaktı. Sunucunun tamponu 1024 bayt, istemci ise 4096 bayta kadar
mesaj gönderiyordu.

**Çözüm:** `common/appProto.c` — her mesajın önüne 4 baytlık uzunluk bilgisi
(big-endian) konuyor. Alıcı kaç bayt bekleyeceğini biliyor. Mesajın **içeriği**
değişmedi; telde hâlâ spesifikasyondaki `<Hello>...</Hello>` ve
`<Invoice>...</Invoice>` metni gidiyor.

Test: 146 KB'lik bir fatura eksiksiz işleniyor ve imzası doğru çıkıyor.

### 4.2 Zaman aşımından sonra bayat yanıt

Bu daha sinsiydi. 30 saniyelik zaman aşımı dolduğunda eski kod bağlantıyı
kullanmaya devam ediyordu. Sunucu geciken yanıtı 35. saniyede gönderdiğinde o
veri soketin tamponunda bekliyor ve **bir sonraki faturanın imzası sanılarak**
`signed/` klasörüne kaydediliyordu.

**Çözüm:** zaman aşımında bağlantı kapatılıp yeniden kuruluyor.

---

## 5. Bağlantı kurtarma

Eskiden `proto_recv` hem zaman aşımında hem bağlantı kopmasında `-1`
döndürüyordu; kullanıcı "sunucu yavaş mı, kapalı mı" ayrımını göremiyordu.

Artık üç ayrı sonuç var: `PROTO_TIMEOUT`, `PROTO_CLOSED`, `PROTO_ERROR`.
İstemci sebebi yazıyor ve saklanan adrese **30 saniye boyunca** yeniden
bağlanmayı deniyor:

```
Upload operation failed
  Sebep: sunucudan 30 saniye icinde yanit gelmedi
  Baglanti sifirlaniyor (gec gelen yanit bir sonraki
  faturanin imzasi sanilmasin diye)
  Yeniden baglanmaya calisiliyor (en fazla 30 saniye)...
   0. saniye - deneme 1: 127.0.0.1:54000 ... basarisiz (hata 10061)
   4. saniye - deneme 2: 127.0.0.1:54000 ... basarili
  Sunucuya yeniden baglanildi, islemi tekrar deneyebilirsiniz.
```

Deneme **sayısı** yerine **süre** sınırlandı. Sebebi: başarısız bir `connect()`
çağrısının ne kadar süreceği önceden belli değil — kapalı bir porta anında
"reddedildi" döner, erişilemeyen bir makineye TCP yeniden denemeleri yüzünden
20 saniyeye kadar bloklanabilir. Sayıya bağlansaydı toplam süre duruma göre 1
saniye de olurdu 60 saniye de.

Sunucu bu pencere içinde geri gelirse istemci kullanıcıdan hiçbir şey istemeden
kendiliğinden bağlanıyor ve `Hello` mesajını sessizce tekrar gönderiyor (sunucu
her yeni bağlantıyı yeni oturum sayıyor).

---

## 6. Kullanılabilirlik hataları

| Sorun | Belirti | Çözüm |
|---|---|---|
| `stdout` tamponlaması | Program tuş beklerken ekranda yarım metin kalıyor, **donmuş gibi** görünüyordu | `setvbuf(stdout, NULL, _IONBF, 0)` |
| Kodlama çatışması | `AYŞE` → `AYÅ`. Dosya adları ANSI, sunucudan gelen XML UTF-8; tek akışta iki kodlama | Konsol UTF-8'e sabitlendi, ANSI metinler yazılmadan önce çevriliyor |
| Çalışma dizini bağımlılığı | Proje kökünden çalıştırılınca fatura dosyaları bulunamıyordu | İstemci açılışta kendi `.exe` konumuna geçiyor |
| Anlaşılmaz bind hatası | Sadece `Bind basarisiz: 10048` | "Port zaten kullanımda, önceki sunucu açık olabilir" açıklaması |

"Donmuş" görünen durum aslında hiç donmamıştı — program işini bitirip
`_getch()` ile tuş bekliyordu, ama kalan çıktı tamponda kalmıştı.

---

## 7. Kod düzeni kuralları

Mentör geri bildirimi doğrultusunda tüm kod gözden geçirildi:

- **Yoda koşulları:** sabit solda (`NULL == fp`, `0 == count`). Yanlışlıkla `=`
  yazılırsa derleyici hata verir; ters yazımda sessizce atama yapılırdı.
- **`static`:** dosya dışından çağrılması gerekmeyen her fonksiyon `static`.
  C'de dosya düzeyindeki isimler varsayılan olarak tüm projeye açıktır.
- **`strncpy`:** `strcpy`'nin sınır kontrolü yok; `strncpy` en fazla n karakter
  kopyalar ama kaynak uzunsa sonuna `'\0'` **koymaz**. Bu gerçek bir hataya yol
  açmıştı — `appUser.c` içinde `nameSurname` kopyalanırken sonlandırma
  unutulmuştu. `copy_text()` ile tek yerde çözüldü.
- **`snprintf`:** `sprintf` yerine her yerde. Kendi kodumuzda `sprintf`,
  `strcpy`, `strcat`, `gets` **sıfır**.

---

## 8. Klasör düzeni

```
client/    InvoiceClient kaynakları, .inv faturalar, upload_system.db, exe
server/    InvoiceServer kaynakları (ağ, imzalama, XML), exe
common/    İki tarafın ortak kullandığı protokol ve konsol katmanı
signed/    Sunucudan dönen imzalı XML'ler
tests/     Otomatik testler
third_party/  sqlite3, mbedTLS, libxml2 (git'e dahil değil)
```

Exe'ler kendi klasörlerine derleniyor, kökte `.bat` kısayolları var. Proje
kökünden `.\InvoiceServer` ve `.\InvoiceClient` yeterli.

`common/` klasörü sizin seçtiğiniz `client/ + server/ + signed/` düzeninden bir
sapma. Sebebi: protokol çerçeveleme kodu iki tarafta **birebir aynı** olmak
zorunda; kopyalamak sessiz uyumsuzluk riski demekti.

---

## 9. Testler

Toplam **76 otomatik kontrol**, üç dosyada:

| Dosya | Kontrol | Kapsam |
|---|---|---|
| `tests/test_server.py` | 22 | Protokol, hash, AES, XML geçerliliği, hatalı girdiler |
| `tests/test_client.py` | 39 | Login, liste, logout, audit, yükleme, **zaman ölçümleri** |
| `tests/test_reconnect.py` | 15 | Bağlantı kopması ve kurtarma |

### Kripto bağımsız doğrulanıyor

`test_server.py`, sunucunun ürettiği hash ve imzayı Python'un bağımsız
`hashlib` / `cryptography` kütüphaneleriyle **yeniden hesaplayıp**
karşılaştırıyor. Yani kod kendi kendini doğrulamıyor.

### Zaman kısıtları ölçülüyor

Mentörün vurguladığı beş senaryonun ölçülen değerleri:

| Senaryo | Beklenen | Ölçülen |
|---|---|---|
| "Login Succeeded" beklemesi | 2 sn | 2.05 sn |
| "Login failed" beklemesi | 2 sn | 2.00 sn |
| "record not found" beklemesi | 2 sn | 2.01 sn |
| Fatura içeriği gösterimi (Faz 1) | 10 sn | 10.01 sn |
| Zaman aşımı → "Upload operation failed" | 30 sn | 30.00 sn |

### İstemci nasıl test edilebilir hale geldi

Menüler `_getch()` ile tek tuş okuduğu için istemci borudan beslenemiyordu; bu
yüzden süre kontrolleri elle yapılıyordu. `common/appConsole.c`, `stdin` bir
boruya yönlendirilmişse satırdan okuyor. **Uygulamanın davranışı değişmiyor**,
sadece girdinin geldiği yer değişiyor.

---

## 10. İsterler listesindeki eksikler

`Proje_Isterleri_Checklist_Test_Guncellemeli.pdf` ile karşılaştırıldığında üç
eksik bulundu:

1. **"...file has been sent, waiting for registration"** — bu checklist mesajı
   *istemci* ekranında istiyor, Phase 2 PDF'i ise *sunucu* ekranında. İki
   doküman çelişiyor; ikisinde de gösteriliyor.
2. **Faz 1 davranışı** (içeriği 10 saniye ekranda tutma) Faz 2 tarafından
   değiştirildiği için silinmişti, ama checklist ikisini de ayrı madde sayıyor.
   `--phase1` seçeneğiyle korundu.
3. **Zaman kısıtları ölçülmüyordu** — yukarıda anlatıldı.

Ayrıca fatura listesinde firma faturalarında soyad büyütülmüyor. Spesifikasyonun
örnek listesi `3 - Hızlı Ticaret` diyor (büyütülmemiş), çünkü firma. Ayrım
faturadaki `<customer type="SAHIS">` alanından yapılıyor.

---

## 11. Çözülmemiş / dikkat edilecekler

**Spesifikasyon çelişkisi:** Adım 3.1 Hello'ya `"OK"` dönülmesini, madde 1 ise
`<Response>I am ready</Response>` istiyor. Telde `I am ready` gönderiliyor —
madde 1 mesajın birebir metnini verdiği için o esas alındı. Hocanıza
sorulabilir.

**İki makine testi** yapılmadı. Kod hazır (`INADDR_ANY` ile dinliyor, IP
kullanıcıdan alınıyor) ama iki fiziksel makinede denenmedi. Adımları
`CHECKLIST.md` sonunda, güvenlik duvarı kuralı dahil.

**AES ECB modunda.** Spesifikasyon yalnızca anahtarı veriyor, mod belirtmiyor.
ECB aynı girdi için hep aynı çıktıyı verdiği için imza tekrar üretilebilir
oluyor, ama genel amaçlı şifreleme için uygun bir mod değildir.

**Hello mesajı şifreyi taşıyor.** Spesifikasyon açıkça böyle istiyor
(`<Hello>logged user name and password</Hello>`). Sunucu şifreyi hiçbir yere
yazmıyor, ama trafik şifresiz.

---

## Commit geçmişi

| Commit | İçerik |
|---|---|
| `5dc7c3e` | Yapı, kütüphaneler, XML düzeltmesi, protokol, hata düzeltmeleri |
| `545a535` | Üç otomatik test paketi |
| `15b9911` | Python bytecode önbelleğinin repodan çıkarılması |

Çalışma `phase2-xml-libraries-and-tests` dalında. `main`'e almak için:

```bash
git checkout main
```

```bash
git merge phase2-xml-libraries-and-tests
```
