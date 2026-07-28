# Proje Gereksinimleri Kontrol Listesi

`Proje_Isterleri_Checklist_Test_Guncellemeli.pdf` maddelerinin birebir karşılığı.

**Doğrulama yöntemi:**

| İşaret | Anlamı |
|---|---|
| `[oto-c]` | `python tests/test_client.py` ile doğrulanıyor — **39/39 geçti** |
| `[oto-s]` | `python tests/test_server.py` ile doğrulanıyor — **22/22 geçti** (`--slow` dahil) |
| `[oto-r]` | `python tests/test_reconnect.py` ile doğrulanıyor — **15/15 geçti** |
| `[elle]` | Elle yapılması gereken (aşağıda adımları var) |

Spesifikasyonun istediği gibi: her kod değişikliğinden sonra liste temizlenip
testler yeniden çalıştırılır.

---

## FAZ 1: InvoiceClient Temel İsterleri

### Genel Gereksinimler
- [x] Uygulama C ile geliştirildi
- [x] Konsol uygulaması (Windows)

### 1. "Login" İşlemi
- [x] Kullanıcıdan "user id" ve "password" isteniyor `[oto-c]`
- [x] Adım 1.1: "Enter User Id:" yazıyor `[oto-c]`
- [x] Adım 1.2: "Enter Password" yazıyor `[oto-c]`
- [x] Adım 1.3: `upload_system.db` / `users` tablosuna SQLite ile erişiliyor `[oto-c]`
- [x] Adım 1.4: ID eşleşmesi aranıyor, sonuca göre dallanıyor `[oto-c]`
- [x] Adım 1.4.a: "Login Succeeded" **2 saniye**, sonra Adım 5 `[oto-c]`
- [x] Adım 1.4.b: "Login failed" **2 saniye**, sonra Adım 1.1 `[oto-c]`
- [x] Adım 5: `audit.LoginTime` kaydediliyor `[oto-c]`
- [x] Adım 5: "Select Operation" menüsü (kullanıcı adı, 1-Invoice List, 2-Logout) `[oto-c]`

### 2. "Invoice List" İşlemi
- [x] `.inv` dosyaları bulunup numaralı liste olarak gösteriliyor (`1 - Hakan USLU`) `[oto-c]`
- [x] ESC → "Select Operation" menüsüne dönülüyor `[oto-c]`
- [x] Geçerli numara (Faz 1) → içerik **10 saniye** ekranda, sonra menü `[oto-c]`
- [x] Geçersiz numara → "record not found" **2 saniye**, sonra menü `[oto-c]`

> Faz 2 bu davranışı değiştiriyor (dosya sunucuya gönderiliyor). İki davranış da
> korundu: varsayılan Faz 2, `--phase1` parametresiyle Faz 1.

### 3. "Logout" İşlemi
- [x] Başlangıç ekranına ("Enter User Id") dönülüyor `[oto-c]`
- [x] `audit.LogoutTime` güncelleniyor `[oto-c]`

### Proje Mimarisi (Faz 1)
- [x] En az 3 kaynak + 2 başlık: `appMain.c`, `appUser.c/h`, `appInvoice.c/h`
- [x] Login/Logout `appUser.c`'de
- [x] Fatura listeleme `appInvoice.c`'de
- [x] İşlem seçim menüsü `appMain.c`'de

---

## FAZ 2: Ağ İletişimi ve InvoiceServer

### 1. Bağlantı Gereksinimleri
- [x] Sunucu açılışta TCP başlatıp IP/Port yazdırıyor `[oto-s]`
- [x] İstemciye TCP eklendi, login sonrası IP ve Port soruluyor `[oto-c]`
- [x] `<Hello>logged user name and password</Hello>` gönderiliyor `[oto-s]`
- [x] Sunucu kullanıcı adını gösteriyor `[oto-s]`
- [x] Sunucu `<Response>I am ready</Response>` dönüyor `[oto-s]`
- [x] İstemci bu yanıtı ekranda gösteriyor `[oto-c]`
- [ ] **Sunucu ve istemci farklı makinelerde çalışmalı** `[elle]` — kod hazır
      (`INADDR_ANY` ile dinliyor, IP kullanıcıdan alınıyor), ama iki fiziksel
      makinede henüz denenmedi

### 2. Fatura Yükleme
- [x] Dosya seçilince içerik ekrana yazdırılmıyor, Invoice mesajı gönderiliyor `[oto-c]`
- [x] İstemci "...[dosya adı]... file has been sent, waiting for registration"
      gösteriyor `[oto-c]`
- [x] Sunucu da aynı satırı kendi ekranında gösteriyor `[oto-s]`
- [x] Yanıt geldiğinde istemci veriyi ekrana yazdırıyor `[oto-c]`

> Not: İki doküman burada çelişiyor. Bu checklist mesajı **istemcide**,
> Phase 2 PDF'i ise **sunucuda** istiyor. İkisinde de gösteriliyor.

### 3. Fatura İmzalama (Sunucu)
- [x] Adım 3.1: İstek tipi kontrol ediliyor (Hello / Invoice / bilinmeyen) `[oto-s]`
- [x] Adım 3.2: İçeriğin SHA-256 HASH'i hesaplanıyor `[oto-s]`
- [x] Adım 3.3: HASH `12345678901234567890123456789012` anahtarıyla AES-256 `[oto-s]`
- [x] `<invoiceStatus date="YYYY-MM-DD hh:mm:ss">` düğümü ekleniyor `[oto-s]`
- [x] `<message>Fatura Kaydedildi</message>` `[oto-s]`
- [x] `<signature>` = AES ile şifrelenmiş hash `[oto-s]`
- [x] İmzalı XML istemciye geri gönderiliyor `[oto-s]`

> Adım 3.1 "OK dönülmeli" diyor, madde 1 ise `<Response>I am ready</Response>`
> istiyor. Telde `I am ready` gönderiliyor — madde 1 mesajın birebir metnini
> verdiği için o esas alındı; ikisi de aynı "istek alındı" onayını tarif ediyor.

### 4. Sunucu Dil Seçimi
- [x] **Sunucu C ile yazıldı** → yukarıdakiler yeterli, ek görev yok
- [x] Java isterleri (mobil cihaz, Connection Form, Invoice Form) geçerli değil

---

## Mentörün Vurguladığı Test Senaryoları

Hepsi otomatik ve **ölçülerek** doğrulanıyor (son çalıştırmadaki değerler):

| Senaryo | Beklenen | Ölçülen | Nasıl |
|---|---|---|---|
| 30 sn zaman aşımı → "Upload operation failed" | 30 sn | **30.00 sn** | `[oto-c]` `[oto-s]` |
| "Login Succeeded" beklemesi | 2 sn | **2.05 sn** | `[oto-c]` |
| "Login failed" beklemesi | 2 sn | **2.00 sn** | `[oto-c]` |
| "record not found" beklemesi | 2 sn | **2.01 sn** | `[oto-c]` |
| Fatura içeriği gösterimi (Faz 1) | 10 sn | **10.01 sn** | `[oto-c]` |

Zaman aşımı testi sunucu `--delay` parametresiyle başlatılarak yapılıyor;
spesifikasyonun istediği "sunucu tarafına geçici gecikme ekle" maddesi bu
şekilde kalıcı ve tekrarlanabilir hale getirildi.

Ölçüm toleransı ±0.6 saniye (süreç başlatma payı).

---

## Testleri çalıştırma

```bash
python tests/test_client.py
```

```bash
python tests/test_server.py --slow
```

```bash
python tests/test_reconnect.py
```

`test_client.py` istemciyi gerçekten çalıştırıp çıktısını zaman damgasıyla
okur ve süreleri ölçer. `test_server.py` hash ve imzayı Python'un bağımsız
`hashlib` / `cryptography` kütüphaneleriyle yeniden hesaplayıp karşılaştırır,
yani kod kendi kendini doğrulamaz.

---

## Ek doğrulamalar (isterlerde yok, kendi eklediklerimiz)

- [x] İmzalı yanıt well-formed XML, `<invoiceStatus>` kök elemanın **içinde** `[oto-s]`
- [x] İmza, bağımsız SHA-256 + AES-256-ECB hesabıyla birebir aynı `[oto-s]`
- [x] Faturanın orijinal alanları imzalamadan sonra korunuyor `[oto-s]`
- [x] Aynı fatura tekrar imzalanınca `invoiceStatus` çoğalmıyor `[oto-s]`
- [x] Geçersiz XML içeren fatura reddediliyor, sunucu çökmüyor `[oto-s]`
- [x] Bilinmeyen mesaj tipi sunucuyu çökertmiyor `[oto-s]`
- [x] ~146 KB'lik fatura eksiksiz işleniyor (mesaj çerçeveleme) `[oto-s]`
- [x] Firma faturasında soyad büyütülmüyor (`3 - Hizli Ticaret`) `[oto-c]`
- [x] Sunucu kapalıyken hata sebebi yazılıyor ve yeniden bağlanılıyor `[oto-r]`
- [x] Zaman aşımından sonra bağlantı sıfırlanıyor (geç gelen yanıt bir sonraki
      faturanın imzası sanılmıyor) `[oto-r]`
- [x] Yeniden bağlandıktan sonra Hello tekrar gönderiliyor `[oto-r]`
- [x] Yeniden bağlanma penceresi ~30 saniye (ölçülen 30.3 sn) `[oto-r]`
- [x] Sunucu pencere içinde geri gelirse istemci kendiliğinden bağlanıyor `[oto-r]`

---

## Elle yapılacak tek iş: iki makine testi

Spesifikasyonun "sunucu ve istemci farklı makinelerde çalışmalı" maddesi.

**Makine A (sunucu):**

```bash
.\InvoiceServer
```

Yazdığı IP'yi not edin (`192.168.x.x`). Windows Güvenlik Duvarı ilk çalıştırmada
izin soracaktır — **özel ağlarda izin verin**. Sormazsa yönetici olarak:

```bash
netsh advfirewall firewall add rule name="InvoiceServer" dir=in action=allow protocol=TCP localport=54000
```

**Makine B (istemci):** aynı ağa bağlı olmalı.

```bash
.\InvoiceClient
```

Login sonrası `Enter Server IP` sorusuna **Makine A'nın IP'sini** girin
(`127.0.0.1` değil), port `54000`.

Kontrol edilecekler: Makine A'nın ekranında kullanıcı adı ve
"file has been sent, waiting for registration" görünmeli; Makine B'de imzalı
XML basılmalı ve `signed/` klasörüne düşmeli.
