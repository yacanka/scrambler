# Scrambler

Windows, Linux ve macOS'ta calisan, surukle-birak destekli bir ZIP
donusturme uygulamasidir. ZIP dosyasinin tum bitlerini dairesel olarak
3 bit kaydirir ve ayni klasorde `_scrambled` son ekli yeni bir dosya
olusturur. Daha once uygulamayla donusturulmus bir dosya tekrar
birakildiginda ters kaydirma uygulanir ve `_restored` son ekli dosya
uretilir.

Orijinal dosya degistirilmez ve cikti boyutu girdi boyutuyla ayni kalir.

> **Guvenlik notu:** Bu islem sifreleme degildir. Gizlilik, butunluk veya
> kimlik dogrulama saglamaz; sabit bit kaydirma kolayca tersine
> cevrilebilir. Hassas verileri korumak icin kullanilmamalidir.

## Arayuz ve UX

- Yeniden boyutlandirilabilir, yuksek DPI destekli masaustu penceresi
- Animasyonlu ve coklu dosya destekli surukle-birak alani
- Basari/hata durumunu gosteren, yumusakca kaybolan bildirim
- Son bes islemi gosteren gecmis paneli ve `Temizle` butonu
- Uzun dosya adlari icin guvenli kisaltma
- Windows'ta executable uzerine birakilan dosyalari acilista isleyip
  otomatik kapanma
- Sistem fontu bulunamazsa yerlesik fonta otomatik geri donus

## Neden raylib?

ISO C standardinda pencere, surukle-birak veya font cizimi saglayan bir
GUI API'si bulunmaz. SDL2 kaldirildiktan sonra C kod tabanini ve tek
arayuz implementasyonunu korumak icin
[raylib 6.0](https://github.com/raysan5/raylib/releases/tag/6.0)
kullanilir.

raylib:

- C API'sine ve Windows/Linux/macOS masaustu destegine sahiptir.
- Pencere, yuksek DPI, dosya birakma ve 2D cizimi tek katmanda saglar.
- zlib/libpng lisanslidir.
- CMake tarafindan resmi `6.0` commit'i ile sabitlenmistir.
- Ses modulu kapatilarak yalnizca gerekli pencere/cizim bilesenleri
  derlenir.

Kurulu bir raylib 6.0 bulunursa o kullanilir. Bulunamazsa CMake resmi
GitHub deposundan sabitlenmis surumu indirip proje icinde derler. SDL2
ve SDL2_ttf kullanilmaz.

## Proje yapisi

```text
scrambler/
├── CMakeLists.txt
├── README.md
├── include/
│   └── scrambler/
│       └── zip_utils.h       Disariya acik donusum API'si
├── src/
│   ├── main.c                Pencere kurulumu ve uygulama girisi
│   ├── app.c / app.h         Durum, olaylar ve dosya isleme akisi
│   ├── ui.c / ui.h           Yerlesim ve tum arayuz cizimleri
│   ├── system_font.c / .h    Platforma gore sistem fontu bulma
│   └── zip_utils.c           ZIP tespiti ve streaming donusum
└── tests/
    └── test_zip_utils.c      GUI'den bagimsiz cekirdek testleri
```

`scrambler_core` GUI'den bagimsizdir. Bu sayede dosya donusum testleri
raylib indirilmeden de derlenebilir.

## Konsol sürümü

Kökteki `main.c`, harici bağımlılık gerektirmeyen ayrı konsol sürümüdür:

```bash
cc -std=c99 -O2 -Wall -Wextra -Wpedantic main.c -o scrambler
./scrambler
```

Windows/MSVC ile: `cl /O2 /utf-8 main.c /Fe:scrambler.exe`.
Dosyayı terminale sürükleyin veya yolunu yazıp Enter'a basın. Çıkmak
için boş satırda Enter'a basın. Dosya yolları komut satırından da
verilebilir: `./scrambler arsiv.zip`.

İşlem sırasında `Dosya işleniyor…`, yüzdelik çubuk, işlenen/toplam veri
ve tahmini kalan süre gösterilir. Süre, geçen gerçek zaman ve işlenen
baytlardan hesaplanır; ilk ölçüme kadar `--` görünür. Çubuk saniyede en
fazla 10 kez yenilenir. `%100`, çıktı başarıyla kapatıldığında gösterilir.
Çıktı bir dosyaya yönlendirilirse yalnızca başlangıç ve sonuç ilerlemesi
yazılır. Bu gösterge konsol sürümüne aittir.

Konsol regresyon testleri (Python 3, ek paket gerektirmez):

```bash
python3 tests/test_console.py ./scrambler
```

## GNU GCC ile adim adim build (Linux)

### 1. Build araclarini ve sistem kutuphanelerini kur

Debian/Ubuntu:

```bash
sudo apt-get update
sudo apt-get install \
  gcc cmake git \
  libgl1-mesa-dev \
  libx11-dev libxrandr-dev libxi-dev \
  libxcursor-dev libxinerama-dev
```

Fedora:

```bash
sudo dnf install \
  gcc cmake git mesa-libGL-devel \
  libX11-devel libXrandr-devel libXi-devel \
  libXcursor-devel libXinerama-devel
```

Kurulumu kontrol et:

```bash
gcc --version
cmake --version
git --version
```

### 2. GCC ile Release build olustur

Repository kokunde:

```bash
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_COMPILER=gcc
```

Ilk yapilandirmada sistemde raylib yoksa sabitlenmis raylib 6.0 kaynagi
indirilir. Ardindan:

```bash
cmake --build build --parallel
```

### 3. Testleri calistir

```bash
ctest --test-dir build --output-on-failure
```

### 4. Uygulamayi ac

```bash
./build/zip_header_tool
```

ZIP veya daha once donusturulmus dosyalari pencereye surukleyip birakin.
Birden fazla dosya ayni anda birakilabilir.

## Sadece cekirdek ve testleri derleme

GUI veya raylib indirmeden donusum kodunu test etmek icin:

```bash
cmake -S . -B build-core \
  -DSCRAMBLER_BUILD_APP=OFF \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build build-core --parallel
ctest --test-dir build-core --output-on-failure
```

## macOS

Gereksinimler:

```bash
xcode-select --install
brew install cmake git
```

Build:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/zip_header_tool
```

macOS pencere katmani Objective-C/Cocoa kaynaklari da derledigi icin
AppleClang varsayilan ve onerilen derleyicidir.

## Windows (MSYS2 UCRT64)

UCRT64 terminalinde:

```bash
pacman -S --needed \
  mingw-w64-ucrt-x86_64-gcc \
  mingw-w64-ucrt-x86_64-cmake \
  git
```

Build ve test:

```bash
cmake -S . -B build \
  -G "MinGW Makefiles" \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Uygulama `build/zip_header_tool.exe` olarak uretilir. Windows build'inde
`-mwindows` otomatik eklenir; uygulamayla birlikte konsol penceresi
acilmaz.

## Calisma mantigi

1. Ilk dort bayt `PK 03 04`, `PK 05 06` veya `PK 07 08` imzalarindan
   biriyse dosya ZIP kabul edilir.
2. ZIP dosyasi dairesel olarak 3 bit sola kaydirilir ve
   `<ad>_scrambled<uzanti>` yoluna yazilir.
3. ZIP olmayan dosya, uygulamanin daha once urettigi bir dosya kabul
   edilerek 3 bit saga kaydirilir ve `<ad>_restored<uzanti>` yoluna
   yazilir.
4. Her iki islemde de dosya boyutu degismez.

Sola ve saga kaydirma birbirinin tam tersidir. Ancak rastgele bir ZIP
olmayan dosyanin bu uygulamayla uretilip uretilmedigini kanitlayan
metadata yoktur; boyle bir dosyada anlamsiz bir `_restored` cikti
olusabilir.

## Sinirlar

- Ayni cikti yolu zaten varsa mevcut cikti dosyasinin uzerine yazilir.
- Masaustu surumunun donusum cekirdegi standart `fseek`/`ftell` kullandigi
  icin `long` tipinin 32 bit oldugu platformlarda yaklasik 2 GB ve ustu
  dosyalar desteklenmeyebilir. Konsol surumu 64 bit dosya konumlandirma
  kullanir.
- Dosya donusumu su anda senkrondur; cok buyuk dosyalarda islem boyunca
  pencere kisa sureli yanit vermeyebilir.
