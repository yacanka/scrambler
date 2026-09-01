# ZIP Baslik Araci

Pencereye suruklenen bir dosyanin **tum icerigini** dairesel bir bit
kaydirmasindan gecirerek taninmaz hale getirir; taninmayan bir dosya
geldiginde ise ayni kaydirmayi ters yonde uygulayarak orijinal ZIP'i
geri getirmeyi dener. **Orijinal dosya hicbir zaman degistirilmez** ve
**dosya boyutu hic degismez**; sonuc her zaman ayni klasorde, adina
`_scrambled` veya `_restored` eklenmis yeni bir dosya olarak yazilir.

Windows, Linux ve macOS'ta **ayni C kaynak kodundan** derlenir. Pencere,
surukle-birak (drag & drop) ve metin cizimi icin
[SDL2](https://libsdl.org) + [SDL2_ttf](https://github.com/libsdl-org/SDL_ttf)
kullanilir; ikisi de bu uc platformu native olarak destekleyen, C ile
yazilmis, olgun kutuphanelerdir.

## Arayuz

- Koyu temalı, dikey gecisli (gradient) arka plan.
- Ortada, kenarlari yavasca renk degistiren ("nefes alan") kesikli
  cizgili bir birakma alani; icinde vektorel olarak cizilmis bir zip
  simgesi (fermuar zigzag'i) ve yonlendirme metni.
- Bir dosya birakildiginda, sonucu birkaç saniye ekranda kalan renkli
  bir bildirim (yesil = basarili, kirmizi = hata) belirir ve yumusakca
  kaybolur.
- Altta, son 5 islemi (check/X isareti + dosya adlari ile) listeleyen
  kalici bir "Son islemler" paneli ve listeyi bosaltan tiklanabilir bir
  **Temizle** butonu bulunur (hover'da renk degistirir).
- Pencere yeniden boyutlandirilabilir; tum yerlesim orana gore yeniden
  hesaplanir.
- Program herhangi bir font dosyasi tasimaz: isletim sisteminde zaten
  yuklu yaygin bir fontu bulup kullanir (`font_finder.c`). Uygun font
  bulunamazsa uygulama **cokmez**, sadece metinsiz/sekil tabanli bir
  modda calismaya devam eder.

## Nasil calisir

1. Dosya turu: kaynak dosyanin ilk 4 bayti `50 4B 03 04` (`PK\x03\x04`,
   standart ZIP local-file-header imzasi) ise **zip** kabul edilir.
2. Islem, dosyanin **tamamini** dairesel (circular) bir bit dizisi gibi
   ele alip sabit bir miktar (3 bit) kaydirir:
   - **Zip ise** -> butun dosya 3 bit SOLA kaydirilir; sonuc olarak
     sadece baslik degil, dosyanin her bayti degisir ve `PK` imzasi da
     dahil hicbir taninabilir desen kalmaz. Cikti
     `<ad>_scrambled<uzanti>` adiyla yeni bir dosyaya yazilir.
   - **Zip degilse** -> bu araçla daha once bozulmus oldugu varsayilir;
     ayni 3 bit ama SAGA (ters yonde) kaydirma uygulanir. Dosya
     gercekten bu araçla bozulmussa, sonuc orijinal ZIP imzasiyla
     yeniden baslar. Cikti `<ad>_restored<uzanti>` adiyla kaydedilir.
3. Iki yonde de **dosya boyutu birebir aynı kalir** (bayt eklenmez/
   silinmez, sadece bitler kaydirilir). Sonuc hem bildirimde/gecmis
   panelinde hem de konsolda gorunur.

Bu iki islem matematiksel olarak birbirinin tam tersidir: bir zip'i
"scrambled" hale getirip cikan dosyayi tekrar araca surukleyerek
"restored" haline getirdiginizde, orijinal zip'le bayt bayt birebir
ayni veriyi geri alirsiniz. Ancak bu sadece dosya gercekten bu araçla
bozulmussa anlamlidir - rastgele/farkli bir dosyayi "geri yuklemeye"
calismak yine anlamsiz veri uretir (bu beklenen bir durumdur, hata
degildir).

**Bilinen sinir:** SDL2'nin cok platformlu olay API'si, bir dosya
pencerenin uzerinde suruklenirken (birakilmadan once) "uzerine geldi"
bilgisini vermez - yalnizca dosya gercekten birakildiginda olay ureti-
lir. Bu yuzden birakma alaninin "nefes alma" animasyonu her zaman
aktiftir (surukleme anini algilayip ozel olarak vurgulamaz); bu,
platformdan bagimsiz kalabilmek icin bilincli bir tercihtir.

## Dosya yapisi

```
zip_header_tool/
├── CMakeLists.txt
├── README.md
├── src/
│   ├── main.c          SDL2/SDL2_ttf penceresi, olay dongusu, yerlesim
│   ├── ui.h / ui.c      Cizim yardimcilari: yuvarlak kose, kesikli
│   │                    cerceve, zip simgesi, gradient, metin dokusu
│   ├── font_finder.h/.c Platforma gore sistem fontu bulma
│   ├── zip_utils.h      ZIP tespiti ve bit-kaydirma tabanli donusum
│   │                    arayuzu
│   └── zip_utils.c      Dairesel bit-kaydirma (scramble/restore) ve
│                        akis (streaming) tabanli dosya IO mantigi
│                        (SDL'den bagimsiz)
└── tests/
    └── test_zip_utils.c   zip_utils.c icin SDL gerektirmeyen birim
                            testleri
```

`zip_utils.c` bilerek SDL'den tamamen bagimsiz tutuldu; hem test etmesi
kolay olsun hem de ileride farkli bir GUI katmanina gecmek isterseniz
mantik kodunu degistirmeden tasiyabilesiniz diye.

## Derleme

### Linux

```bash
sudo apt-get install libsdl2-dev libsdl2-ttf-dev cmake build-essential
# veya: sudo dnf install SDL2-devel SDL2_ttf-devel cmake gcc   # Fedora
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/zip_header_tool
```

### Windows (MSVC + vcpkg)

```powershell
git clone https://github.com/microsoft/vcpkg
.\vcpkg\bootstrap-vcpkg.bat
.\vcpkg\vcpkg install sdl2:x64-windows sdl2-ttf:x64-windows

cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=vcpkg\scripts\buildsystems\vcpkg.cmake
cmake --build build --config Release
.\build\Release\zip_header_tool.exe
```

MinGW/MSYS2 kullaniyorsaniz:
`pacman -S mingw-w64-x86_64-SDL2 mingw-w64-x86_64-SDL2_ttf mingw-w64-x86_64-cmake`

### macOS

```bash
brew install sdl2 sdl2_ttf cmake
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/zip_header_tool
```

## Testler

`zip_utils.c` icindeki mantik (zip tespiti, bit-kaydirma ile bozma/geri
getirme, dosya boyutunun ve orijinal dosyanin degismemesi) SDL'e ihtiyac
duymayan ayri bir test dosyasiyla dogrulanir. Testler **bilerek
`assert()` kullanmaz**: proje varsayilan olarak Release modunda
derlendigi icin (`NDEBUG` tanimli olabilir), assert() sessizce devre
disi kalabilir - bunun yerine her zaman calisan bir `CHECK` makrosu
kullanilir. Ana senaryo, kucuk/bos/uc-bit-degerli (0x00, 0xFF gibi)/
daha buyuk veri kumeleriyle tam bir "bozup-geri-getirme" (round-trip)
dongusu kurup sonucu orijinalle bayt bayt karsilastirir.

```bash
cmake --build build --target test_zip_utils
ctest --test-dir build --output-on-failure
```

Bu proje gelistirilirken:
- Birim testleri Linux'ta calistirilip gecirildi; ayrica testin
  gercekten hata yakaladigi, mantiga bilerek bir hata sokup testin
  basarisiz oldugu, sonra geri alinip tekrar gectigi dogrulanarak
  test edildi.
- `zip_header_tool` hedefi SDL2 + SDL2_ttf'e karsi uyarisiz (`-Wall
  -Wextra`) derlenip baglandi.
- Arayuz, sanal bir X ekrani (Xvfb) altinda calistirilip ekran
  goruntusu alinarak gorsel olarak dogrulandi: bos durum, basarili
  (yesil) ve hatali (kirmizi) bildirimler, gecmis paneli ve
  **Temizle** butonunun tiklanmasi ayri ayri test edildi. Bu sirada,
  fare hareketiyle tiklamanin ayni olay grubunda geldigi durumlarda
  "Temizle" butonunun bir kare gecikmeyle tepki verdigi fark edildi
  ve yerlesim/olay isleme sirasi duzeltilerek giderildi.
- Bit-kaydirma islemi, gercek bir zip dosyasi uzerinde hem "bozma" hem
  "geri getirme" adimlariyla uctan uca test edildi; cikan baytlar
  `od -An -tx1` ile incelenerek bozulmus dosyanin gercekten farkli
  baytlara sahip oldugu, geri getirilen dosyanin ise orijinalle
  bayt bayt ayni oldugu dogrulandi.
- Cok uzun dosya adlarinin bildirim/gecmis kartlarini tasirdigi
  ayrica fark edildi ve `ui_fit_text` ile (sigmayan metnin sonuna
  "..." eklenerek) duzeltildi; duzeltme, kasitli olarak asiri uzun
  bir dosya adiyla tekrar ekran goruntusu alinarak dogrulandi.
- Gercek isletim sistemi surukle-birak'i (OS-level drag & drop) bu
  gelistirme ortaminda simule edilemedi; bunu kendi masaustunuzde
  denemeniz onerilir.

## Notlar / sinirlar

- Zip tespiti sadece standart local-file-header imzasina (`PK\x03\x04`)
  bakar. Bos arsiv (`PK\x05\x06`) veya "spanned" arsiv (`PK\x07\x08`)
  imzali dosyalar bu haliyle "zip degil" olarak islenir; gerekirse
  `zip_utils.c` icindeki `is_zip_file` fonksiyonuna bu imzalar da
  kolayca eklenebilir.
- "Geri getirme" (restore) islemi yalnizca, verilen dosya gercekten bu
  araçla daha once bozulmussa anlamlidir. Bu araçla ilgisi olmayan,
  rastgele bir "zip olmayan" dosyayi surukleyip birakirsaniz, yine de
  bir cikti dosyasi uretilir ama icerigi anlamli bir zip olmaz - bu,
  hatali bir davranis degil, geri getirmenin dogasi geregidir.
- Bit-kaydirma miktari (`ROTATE_SHIFT_BITS`, varsayilan 3) programa
  sabit olarak gomulmustur; bozma ve geri getirme hep ayni degeri
  kullanir. Degistirmek isterseniz `zip_utils.c` icindeki tanimi
  guncelleyip projeyi yeniden derlemeniz yeterlidir (1-7 arasinda,
  8'e bolunmeyen bir deger olmali).
- Bozma (sola kaydirma) islemi, dosyanin ilk baytini onceden okuyup
  sakladigi icin tek gecişte (streaming) calisir ve buyuk dosyalarda
  da sorunsuzdur. Geri getirme (saga kaydirma) islemi ise dairesel
  sarma icin once dosyanin son baytini okumak zorunda oldugundan bir
  kez dosya sonuna atlar (`fseek`), ardindan normal akisla ilerler;
  bu da sabit (O(1)) ek bellekle calisir, dosyanin tamami belleğe
  yuklenmez. Standart `fseek`/`ftell` (`long`) kullanildigindan,
  32-bit `long` tipine sahip platformlarda (orn. 32-bit Windows
  derlemeleri) yaklasik 2 GB'tan buyuk dosyalarda sinir yasanabilir.
- Ayni klasorde ayni isimde bir dosya zaten varsa uzerine yazilir
  (orijinal surukle-birak edilen dosya degil, uretilen `_scrambled` /
  `_restored` dosyasi).
- Font bulunamazsa arayuz metinsiz sekilde calisir; kendi fontunuzu
  eklemek isterseniz `font_finder.c` icindeki aday yol listesine
  ekleyebilir ya da `main()` icinde sabit bir yol verebilirsiniz.
- Bildirim ve gecmis panelindeki dosya adlari, kartin genisligine
  sigmiyorsa sonuna "..." eklenerek kisaltilir (`ui_fit_text`); tam
  dosya adi her zaman konsol ciktisinda ve uretilen dosyanin kendisinde
  eksiksiz olarak yer alir.
