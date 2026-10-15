> Historical development notes. The root README describes the current source. References to 0.2.0 and pending integration reflect earlier stages.

# Acer Nitro AN517-41: fan sensörleri ve PWM kontrolü

Sürüm 0.2.0 — 15 Ekim 2026. Linux Mint 22.3 / 6.8.0-147-generic, BIOS V1.14 üzerinde canlı doğrulandı.

## Özellikler

CPU ve GPU fanları Acer WMI üzerinden standart Linux hwmon arayüzüne taşınır. DMI sistem üreticisi Acer ve ürün adı Nitro AN517-41 olmalıdır. Hız kontrolü yalnızca test edilen V1.14 BIOS sürümünde ve mod/hedef sorguları başarılı olduğunda açılır; diğer BIOS sürümlerinde sensör okuması kalır.

| Dosya | İşlev | İzin |
|---|---|---|
| fan1_input / fan2_input | CPU / GPU fan devri, RPM | 0444 |
| fan1_label / fan2_label | CPU Fan / GPU Fan | 0444 |
| pwm1 / pwm2 | Hız hedefi, 0–255 ölçeği; bu sürümde yalnızca 128–255 kabul edilir | 0644 |
| pwm1_enable / pwm2_enable | 2: BIOS Auto, 1: manuel, 0: donanım tam hız | 0644 |

Manuel alt sınır %50; fan durdurma desteklenmez. Donanım hedefi tam sayı yüzdeye yuvarlanır, okunan PWM değeri 1 kadar farklı olabilir. PWM bir RPM hedefi değildir. Fan devri ölçümleri saniyede en fazla bir kez yenilenir. Mod ve hedef sorguları önbellekten değil firmware'den okunur.

Manuel moda ilk girişte hedef %100 yapılır; ardından kontrolcü istenen hedefi yazmalıdır. Manuel modu tekrar etkinleştirmek mevcut geçerli hedefi sıfırlamaz. PWM hedefini yazmak tek başına otomatik modu kapatmaz.

## Hazır kontrolcüyle kullanım

CoolerControl gibi hwmon/PWM kullanan araçlara standart arayüz sunulur. CoolerControl bu makinede henüz kurulmadı; uygulamanın kendisiyle uçtan uca test yapılmadı. Kontrolcüde iki fan için minimum hızı en az %50 seçin. Alt değerler sessizce değiştirilmez, EINVAL ile reddedilir. Mevcut sıcaklık sensörleri k10temp ve amdgpu üzerinden kullanılabilir; bu sürücü yeni sıcaklık sensörü oluşturmaz.

Sensörleri terminalde görmek için:

```sh
sensors 'acer_an517-*'
```

hwmon numarası sabit değildir; aygıtı `name=acer_an517` ile bulun. Manuel kullanım gerekmiyorsa iki `pwm*_enable` alanını 2 bırakın.

## Otomatiğe dönüş ve sınırlar

- Firmware durum baytları, yanıt türü/uzunluğu ve yazma sonrası mod/hedef geri okumaları kontrol edilir.
- Mod veya hedef yazma hatasında sürücünün yönettiği fanları Auto'ya döndürme denenir; başarısızlık kernel günlüğüne yazılır.
- Sürücü kaldırılırken ve kapanış/yeniden başlatmada sürücünün başlattığı manuel/tam hız kontrolü Auto'ya döndürülür.
- Uyku/hibernasyon öncesi Auto'ya dönüş istenir. Başarısızsa sürücü uyku isteğine hata döndürür. Uyanmada eski manuel ayar uygulanmaz.
- Kullanıcı alanındaki kontrolcü çökerse modül kendiliğinden kaldırılmaz. Bu sürümde süreç watchdog'u/komut zaman aşımı yoktur; son manuel hedef kalabilir. %50 alt sınır her yükte yeterli soğutma garantisi değildir. Kontrolcünün durdurma davranışı ve hata politikası ayrıca doğrulanmalıdır.
- Uyku, hibernasyon, fiziksel yeniden başlatma, kontrolcü çökmesi ve donanım/WMI hata enjeksiyonu canlı test edilmedi. İlgili geri dönüş kodları bulunması bu senaryoların test edildiği anlamına gelmez.
- Aynı Acer gaming WMI GUID'sini sahiplenen ASense/DAM benzeri sürücülerle eşzamanlı kullanım amaçlanmamıştır. Geçişten önce bu sürücüyü kaldırın.

Kurulum/test sonunda iki fan da Auto'dadır; kalıcı manuel eğri ayarlanmadı.

## Kurulum ve kaldırma

DKMS, çalışan çekirdeğin header dosyaları, make ve C derleyicisi gerekir. Mevcut sistemde bunlar zaten vardı. Sürücü aşamasında ek paket/depo eklenmedi; sonraki CoolerControl kurulumu aşağıda açıklanmıştır.

```sh
sudo sh install.sh
sudo sh uninstall.sh
```

Kurulum önce 0.2.0 kaynağını DKMS ile derler, ardından önceki 0.1.0 sürümünü kaldırıp yenisini yükler. Kaynak `/usr/src/acer-an517-hwmon-0.2.0`, DKMS modülü `acer_an517_hwmon`, açılış ayarı `/etc/modules-load.d/acer-an517-hwmon.conf`. Gelecekteki çekirdeklerde DKMS yeniden derlemeyi dener; API uyumluluğu ve Secure Boot anahtar güveni her sürüm için garanti edilmez. Kurulum hata verirse kısmi 0.2.0 kaydı uninstall.sh ile temizlenebilir. Lisans GPL-2.0-only.

## Canlı doğrulama

Firmware yöntemi 0x0F ile iki fanın başlangıçta Auto olduğu doğrulandı. Yöntem 0x0E mod değişimi, 0x10 hedef yazma, 0x11 hedef okuma ve 0x05 RPM sorgusu kullanıldı. Ham EC/memory erişimi yapılmadı. Firmware'in hedef yazma yanıtı 4 bayt, mod yanıtı 8 bayt olduğundan uzunluklar yönteme göre kontrol edilir.

İlk kısa deneyde %70 hedefte CPU 2290 → 4166 RPM, GPU 2439 → 4615 RPM; tam hızda CPU 5000 RPM, GPU 5769 RPM görüldü. Her iki fanın Auto'ya geri döndüğü ve önceki %100 hedef alanlarının korunduğu doğrulandı.

Standart sysfs üzerinden ayrı entegrasyon testi:

| Kanal | %50 hedef | %70 hedef |
|---|---|---|
| CPU | 3703 RPM | 4166 RPM |
| GPU | 4109 RPM | 4615 RPM |

Geçersiz PWM (127/256), geçersiz mod (3), manuel mod idempotansı, tam hız modu geri okuması, Auto'ya dönüş ve manuel oturum sırasında modül kaldırma sonrası Auto'ya dönüş testleri geçti. Sonuçlar control-validation.json dosyasında. Bu değerler kısa deney örnekleridir, fan kalibrasyon tablosu değildir.

W=1 ile kaynak uyarısı olmadan derlendi. Tam sistem güç geçişi testleri henüz yapılmadı. CoolerControl entegrasyonu aşağıdaki şekilde doğrulandı.

Kaynaklar:
- https://docs.kernel.org/hwmon/hwmon-kernel-api.html
- https://docs.kernel.org/hwmon/sysfs-interface.html
- https://github.com/torvalds/linux/blob/master/drivers/platform/x86/acer-wmi.c
- https://docs.coolercontrol.org/hardware-support.html

## CoolerControl entegrasyonu — 15 Ekim 2026

Resmi apt.coolercontrol.org deposundan CoolerControl ve coolercontrold 5.0.1 kuruldu. Servis aktif ve açılışta etkin; arayüz http://localhost:11987 adresinde. İlk parola kullanıcı tarafından belirlendi. CPU Fan ve GPU Fan otomatik algılandı; ek CoolerControl kodu gerekmiyor.

Arayüzden kanallar ayrı ayrı Manual %70 yapıldı: CPU 4166 RPM, GPU 4615 RPM; sysfs PWM 179 ve enable=1 ile doğrulandı. Ardından her iki kanala Unmanaged uygulanıp enable=2 (firmware Auto) doğrulandı. Son örnek CPU 2290 RPM, GPU 2439 RPM. Auto modunda görünen %70 eski manuel hedef alanıdır, anlık fan yüzdesi değildir.

Manuel aralık sürücüde %50–100; arayüz daha düşük değer seçtirebilse bile sürücü reddeder. Calibrate Fans çalıştırılmadı, özel sıcaklık eğrisi atanmadı. Liquidctl USB desteği bu kurulumda yüklenmedi; ilgili uyarı Acer hwmon kontrolünü engellemiyor. Servis çökmesi/güç geçişi testleri yapılmadı.

## Yayın hazırlığı: yapılandırılabilir alt sınır (henüz canlıya kurulmadı)

Kaynakta `min_fan_percent` modül parametresi eklendi. Varsayılan 50; 0–100 arası kabul edilir, bunun dışındaki değerlerde sürücü bağlanmaz. Parametre yükleme anında seçilir ve sysfs üzerinden salt okunur; anlık olarak değiştirilemez. Her iki fana uygulanır. Örneğin `/etc/modprobe.d/acer-an517-hwmon.conf` içinde:

```conf
options acer_an517_hwmon min_fan_percent=50
```

`min_fan_percent=0` alt sınırı kaldırır ve fan durdurma dâhil 0–100 hedeflerine izin verir; firmware'in bu hedefleri nasıl uyguladığı doğrulanmadı. Bu seçenek deneysel olup soğutma güvenliği sağlamaz. Manuel moda ilk geçiş yine %100 ile başlar; Auto'ya dönüş davranışı korunur. CoolerControl kalibrasyonu bu seçenek açıldığında da doğrulanmış sayılmaz.

Bu kaynak değişikliği derleme ile kontrol edildi; sistemde yüklü 0.2.0 hâlâ sabit %50 sınırını kullanıyor. Yeni parametrenin canlı donanım testi ve sürüm artırımı yayın öncesinde yapılmalıdır. Önceki JSON sonuçları kurulu sürüme aittir.
