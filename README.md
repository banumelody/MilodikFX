# MilodikFX

MilodikFX adalah prosesor multi-efek gitar/bass realtime untuk Windows, dibuat untuk rig satu orang.
Satu executable C++20/JUCE mandiri: menjalankan audio engine, menyajikan HTTP API loopback, dan
menampilkan UI React **di dalam jendelanya sendiri** lewat Edge WebView2 — tanpa tab browser, tanpa
proses UI terpisah. Bundle UI tertanam di dalam exe, jadi binernya berdiri sendiri.

Target kedua membangun rantai DSP yang sama sebagai **plugin VST3** plus wrapper JUCE Standalone —
**dengan UI yang sama persis**, disajikan langsung dari dalam biner plugin, tanpa membuka socket apa pun.

## Unduh

Build siap pakai ada di [halaman Releases](https://github.com/banumelody/MilodikFX/releases):

- **`MilodikFX-x.x.x-setup.exe`** — installer biasa, paling mudah dibagikan.
- **`MilodikFX-x.x.x-portable.exe`** — exe tunggal tanpa dipasang; WASAPI (shared/exclusive/low-latency)
  dan DirectSound. Jalan di Windows mana pun.
- **`MilodikFX-x.x.x.exe`** — dengan dukungan ASIO, latensi paling rendah.
- **`MilodikFX-x.x.x-VST3.zip`** — plugin VST3 untuk dipakai di DAW.

Taruh di folder mana saja lalu jalankan. UI terbuka di jendelanya sendiri (butuh WebView2, sudah ada di
Windows 10/11 modern).

## Cara memasang

### Cara termudah — installer

Jalankan **`MilodikFX-x.x.x-setup.exe`**. Di halaman komponen, biarkan
**"Pasang plugin VST3"** tercentang, lalu Next sampai selesai. Itu saja: aplikasi dan plugin
terpasang sekaligus, dan plugin diletakkan di folder yang **sudah dipindai setiap DAW**, jadi tidak
ada path yang perlu diatur.

Installer tidak butuh hak administrator. Kalau dijalankan biasa, plugin masuk ke folder VST3
milikmu sendiri; kalau dijalankan sebagai administrator, ke folder sistem. Keduanya dipindai host.

> **Tutup DAW-mu dulu.** Selama sebuah host masih memuat `MilodikFX.vst3`, Windows mengunci
> berkasnya dan installer akan gagal menimpanya. Ini berlaku untuk semua plugin, bukan cuma ini.

Mencopot lewat Settings → Apps akan membersihkan keduanya — **preset di `Documents\MilodikFX`
tidak ikut terhapus.**

### Aplikasi saja, tanpa dipasang

Unduh **`MilodikFX-x.x.x.exe`** (ASIO) atau **`MilodikFX-x.x.x-portable.exe`** (WASAPI/DirectSound),
taruh di folder mana saja, lalu jalankan. Tidak ada yang perlu dipasang.

### Plugin saja, manual

Kalau kamu memakai exe portable dan tetap ingin plugin-nya, unduh
**`MilodikFX-x.x.x-VST3.zip`**, ekstrak, lalu salin folder `MilodikFX.vst3` ke **salah satu**:

| Lokasi | Untuk |
|---|---|
| `%LOCALAPPDATA%\Programs\Common\VST3` | Hanya untukmu — **tanpa perlu admin** |
| `C:\Program Files\Common Files\VST3` | Semua pengguna di komputer ini (butuh admin) |

Tempel path itu ke address bar Explorer. Buat foldernya kalau belum ada.

Lalu di DAW-mu: **pindai ulang plugin** dan cari `MilodikFX`.
Di Reaper: *Options → Preferences → Plug-ins → VST → Re-scan*.

> **Yang dibutuhkan:** Windows 10 1809 ke atas, dan **Microsoft Edge WebView2 Runtime** untuk
> tampilannya — sudah ada di Windows 11 dan Windows 10 modern. Installer akan memberi tahu kalau
> tidak terdeteksi.

## Di dalam DAW (VST3)

Plugin memakai **rantai, UI, preset, dan file IR/NAM yang sama** dengan aplikasi — sound yang kamu
bangun di panggung terbuka apa adanya di studio, karena keduanya membaca folder
`Documents\MilodikFX` yang sama.

Yang berbeda di dalam DAW, dan semuanya disengaja:

- **Tempo ikut host.** Delay tersinkron dan LFO terkunci-tempo mengikuti tempo proyek, bukan angka
  yang diketik di plugin.
- **Bypass host memakai crossfade 10 ms** milik engine, bukan potongan keras.
- **Latensi dilaporkan dengan benar** — oversampling overdrive plus resampler NAM — dan diperbarui
  saat model dimuat, jadi PDC DAW-mu tepat. Di sesi **48 kHz** resampler NAM jadi passthrough total:
  nol interpolasi, nol latensi tambahan.
- **Modifier jalan**, termasuk **pedal ekspresi** lewat MIDI dari host — wah, tremolo, dan auto-wah
  hidup di DAW. Kirim CC pedalmu ke track plugin-nya.
- **Tanpa looper, metronom, pemilih device, dan panel MIDI.** DAW sudah punya semuanya, dan looper
  akan menyita 23–46 MB per instance untuk fitur yang tak punya kontrol di sana. Pemetaan footswitch
  ke scene/channel juga belum ada di plugin (di aplikasi ada).

Divalidasi dengan `pluginval --strictness-level 10` (tingkat tertinggi) di CI setiap push.

## Rantai sinyal

```
InputTrim → NoiseGate → CleanBoost → Compressor → Overdrive → EQ → Contour → NAM → Cabinet → Delay → Reverb → MasterOut
```

Semuanya untuk **gitar maupun bass**. Sorotan:

- **Input Gain** di depan noise gate — samakan level tiap gitar/bass sekali, gate ikut menyesuaikan.
- **Overdrive** dengan 11 voicing pedal — dari overdrive transparan sampai distorsi dan fuzz
  (Tube Screamer, Bluesbreaker, Blues Driver, Transparent/Klon, OCD, Dumble, Marshall-in-a-Box,
  Clean Boost, Centaur, RAT, Big Muff); kontrol menyesuaikan tipe, plus asimetri + oversampling.
- **Amp (NAM)** — kepala amp hasil capture Neural Amp Modeler (`.nam`), di antara tone shaping dan
  cabinet. Melengkapi cabinet IR: IR memodelkan speaker, NAM memodelkan kepala amp-nya. Butuh CPU AVX2.
- **Cabinet** analitik + dua slot impulse response yang bisa di-blend.
- **Delay** dengan damping, ping-pong, dan sinkron tempo; **Reverb** algoritmik/konvolusi. Keduanya
  punya spillover — ekornya tetap meluruh saat dimatikan, jadi pindah scene tidak memotong repeat.
- **Metronome** dicampur setelah master (di luar bypass), berbagi satu tempo dengan delay.
- **Looper**: rekam satu frasa lalu bermain di atasnya (overdub berlapis), dicampur setelah master jadi
  loop tetap berbunyi walau bypass. Bisa dikendalikan lewat footswitch.

**Urutan rantai bisa ditata ulang** — seret kartu rack atau chip di strip rantai (atau pakai tombol
↑▼, atau keyboard: Enter angkat, panah pindah). Sepuluh dari dua belas blok bebas digeser; Input dan
Master terkunci karena meter mengandalkan posisi trim dan Master membawa limiter pengaman. Urutannya
ikut tersimpan di preset.

**Split A/B**: blok **Split** membelah sinyal jadi dua jalur, blok **Mixer** menggabungkannya lagi
dengan level dan pan sendiri per jalur. Tiap blok di antaranya tinggal ditandai A atau B — tanpa
duplikasi blok, jadi preset lama tetap utuh. Mode **crossover** (Linkwitz-Riley) mengirim low ke satu
jalur dan high ke jalur lain: low bersih, high di-drive, gerakan klasik rig bass. Mati secara default,
dan saat mati rantainya bit-identik dengan sebelumnya.

**Blok bisa lebih dari satu.** Tiga overdrive, dua dari hampir semua yang lain - tumpuk dua drive
seri di satu jalur, atau taruh satu di tiap jalur dengan kenop yang sepenuhnya terpisah. Daftar
**Blok** menunjukkan sisa jatah tiap tipe (`2/3`). **Amp (NAM) tetap satu**: itu batas CPU, bukan
batas format - satu model Standard sudah sekitar 29% budget di 96 kHz.

Setelah update, semua yang kamu punya tetap jalan: **instance pertama memakai id yang sama seperti
dulu**, jadi preset, settings, pemetaan MIDI, dan lajur otomasi DAW lama tidak perlu dimigrasi sama
sekali. Yang baru cuma bernomor - `overdrive2`, `overdrive3`.

**Board bisa dikosongkan.** Tiap blok bisa dibuang dari board lewat tombol x di kartunya, dan diambil
lagi dari daftar **Blok** di kanan - seret ke rack, atau tekan Enter untuk menaruhnya di ujung rantai.
Board kosong bukan keadaan khusus: sinyalnya lewat lurus, seperti *shunt* di grid Fractal. Membuang
blok tidak sama dengan mem-bypass-nya - delay yang di-bypass tetap berjalan supaya ekornya meluruh,
delay yang dibuang tidak ada sama sekali. **Splitter** membuka jalur kedua dan **Mixer** menyusul
sendiri, lalu ikut hilang saat Splitter dibuang. Input dan Master tidak bisa dibuang.

Setelah update, board-mu tetap utuh: preset dan settings yang ditulis sebelum v0.30 tidak menyebut
board sama sekali, dan itu dibaca sebagai **semua blok terpasang**.

**Meter menampilkan L dan R terpisah**, di input maupun output — berguna begitu kedua sisi
benar-benar bisa berbeda (pan per jalur, Cabinet stereo, mode split L/R). Di input itu langsung
memperlihatkan sumber mana yang jauh lebih panas sebelum kamu menyetel apa pun. **Gain input juga
bisa dilepas kaitannya** supaya tiap kanal punya trim sendiri; secara default terkait, jadi rig mono
dan preset lama berperilaku sama persis.

**Gitar dua jack** (mis. pickup magnetik + piezo): pilih port fisik untuk kanal L dan R di panel
Audio Device, lalu pakai mode split **L/R** — kanal L ke jalur A, kanal R ke jalur B. Hasilnya dua
chain penuh dengan kenop masing-masing, digabung di Mixer. Stereo saja tidak cukup untuk ini: satu
overdrive memproses kedua kanal dengan **nilai kenop yang sama**, jadi L dan R baru bisa disetel
berbeda kalau masing-masing punya jalur sendiri. **Invert B** di Mixer membalik polaritas jalur B,
untuk dua pickup yang saling meniadakan sebagian saat diblend.

Pemilihan port melekat pada **perangkat**, bukan preset — ia menggambarkan kabel di rig, jadi preset
tetap bermakna di interface lain.

Kenopnya punya **tanda skala** dan **tanda tengah** untuk parameter bipolar, dan **kontrol utama tiap
blok digambar lebih besar** — hierarki yang terbaca sebelum labelnya dibaca. **Level A/B di Mixer
adalah fader**, satu-satunya di aplikasi ini: itu fungsi console, dan membandingkan dua tinggi jauh
lebih mudah daripada dua sudut putar.

**Parameter waktu dan frekuensi punya travel logaritmik.** Attack compressor 0.1–200 ms dulu menaruh
seluruh wilayah bergunanya di 2,5% pertama gerakan; sekarang tersebar merata. dB tetap linear karena
sudah logaritmik terhadap besaran yang diwakilinya. Nilai tersimpan, preset, dan otomasi DAW tidak
berubah sama sekali — yang berubah hanya seberapa jauh kamu menarik.

Tampilannya **material, bukan ilustrasi**: kartu terbaca sebagai enclosure dan kenopnya terasa
seperti benda yang diputar — semuanya CSS bersama, jadi blok baru tidak menagih gambar apa pun.
Hanya **Cabinet** dan **Amp** yang punya permukaan sungguhan, karena keduanya tunggal dan wujudnya
tidak berubah menurut parameter. Perform view sengaja tetap polos supaya terbaca dari jauh.

Fitur kontrol ala rig panggung: **channel A/B/C/D** per efek (empat sound bernama tiap blok), **scene**
4 slot yang membawa channel tiap efek, **Perform view** (layar besar untuk manggung — tombol scene
raksasa dengan huruf channel, tuner besar, kontrol looper, pintasan keyboard), dan **modifier** — LFO
(bisa dikunci ke tempo), envelope, atau pedal ekspresi menyapu parameter (tremolo, auto-wah, wah);
knob-nya tetap aktif menyetel titik tengah sapuan.

**Kontrol MIDI/footswitch** dengan MIDI Learn ke parameter, scene, channel, atau aksi looper;
**wizard 4-tombol** memasang footswitch (mis. M-Vave Chocolate) ke Scene 1–4 sekali jalan; **USB dan
Bluetooth LE MIDI** (lewat backend WinRT); dan **auto-reconnect** — controller wireless yang tidur
nyambung lagi sendiri.

Fitur lain: **tuner kromatik** (gitar & bass 5-senar, sampai low B ≈ 31 Hz), **preset** dengan metadata
+ impor/ekspor (**10 preset gitar & bass sudah terpasang** — jazz, blues, rock, metal, ambient, funk,
fingerstyle, dub), **knob tersemat** — sampai delapan kontrol per preset yang muncul besar di Perform
view, **undo/redo**, kurva respons EQ, metering lewat Server-Sent Events, dan **pengecekan
update otomatis** — aplikasi memeriksa GitHub Releases saat dibuka dan memunculkan pemberitahuan bila
ada versi baru.

Situs: **https://banumelody.github.io/MilodikFX/**

## Build (Windows)

Frontend harus dibangun **sebelum** CMake configure, karena bundle-nya ditanam ke exe saat configure.

```powershell
# Semuanya, termasuk installer bila Inno Setup ada:
powershell -ExecutionPolicy Bypass -File scripts\build-release.ps1

# ...atau manual:
cd frontend; npm ci; npm run build; cd ..
cmake -S . -B build -G "Visual Studio 17 2022" -A x64   # JUCE, WebView2, NAM di-fetch saat pertama
cmake --build build --config Release --parallel
build\MilodikFX_artefacts\Release\MilodikFX.exe
```

Prasyarat: CMake 3.25+, Visual Studio 2022 (MSVC), Node.js untuk frontend. Koneksi internet dibutuhkan
saat configure pertama (JUCE, WebView2, dan NeuralAmpModelerCore di-fetch via CMake).

**ASIO (opsional, latensi terendah).** Pasang Steinberg ASIO SDK lalu set `ASIOSDK_DIR`; CMake
mendeteksinya dan mengaktifkan ASIO otomatis. SDK tidak disertakan di repo (lisensi Steinberg mengizinkan
pemakaian, bukan redistribusi). Tanpa SDK, engine jatuh ke WASAPI exclusive.

**NAM (opsional).** Aktif secara default (`-DMILODIKFX_ENABLE_NAM=OFF` untuk mematikannya). Model
membutuhkan CPU AVX2; di CPU lama, pemuatan model ditolak dengan pesan jelas dan aplikasi tetap jalan.

Log dan settings ada di `%APPDATA%\MilodikFX\`. Preset, impulse response, dan model NAM ada di bawah
`Documents\MilodikFX\`.

## Tes

```powershell
# Backend (JUCE UnitTest diagregasi jadi satu biner)
cmake --build build --config Debug --target MilodikFX_tests
build\MilodikFX_tests_artefacts\Debug\MilodikFX_tests.exe
ctest --test-dir build -C Debug --output-on-failure

# Frontend
cd frontend
npx vitest run       # catatan: `npm run test` masuk watch mode
npm run type-check
npm run lint

# End-to-end terhadap engine sungguhan
powershell -ExecutionPolicy Bypass -File .github\scripts\run-local-e2e.ps1 [-Build]
```

## Dukung proyek ini ☕

MilodikFX gratis dan sumber terbuka — dikerjakan satu orang di sela-sela waktu, tanpa iklan dan tanpa
langganan. Kalau ia menghemat uangmu dari beli pedal atau plugin, atau sekadar bikin latihan dan rekaman
lebih asyik, pertimbangkan **mentraktir satu kopi**. Dukungan sekecil apa pun bikin voicing berikutnya,
perbaikan bug, dan rilis baru terus jalan.

> ☕ **[Jadi sponsor / traktir kopi lewat GitHub Sponsors](https://github.com/sponsors/banumelody)**

Nggak harus banyak — segelas kopi pun sudah bikin ngoding sampai larut terasa lebih ringan, dan jadi
alasan buat merilis fitur berikutnya lebih cepat. Terima kasih sudah memakai MilodikFX! 🙌

## Lisensi

MilodikFX berlisensi **GNU Affero General Public License v3.0** — lihat [LICENSE](LICENSE).

Pilihan itu bukan sembarangan. Biner me-link JUCE, yang modul-modulnya dwi-lisensi AGPLv3 atau lisensi
komersial JUCE; mendistribusikan build tanpa lisensi komersial berarti karya gabungannya AGPLv3. Build
yang mengaktifkan ASIO menyertakan Steinberg ASIO SDK (perjanjian Steinberg atau GPLv3 — §13 GPLv3
mengizinkan penggabungan dengan AGPLv3). NAM core (MIT), Eigen (MPL2), dan nlohmann/json (MIT) semuanya
kompatibel dengan AGPLv3. Model `.nam` adalah data yang dimuat saat runtime, bukan kode ter-link, jadi
lisensinya tidak menular ke aplikasi.

Praktisnya: kamu boleh memakai, memodifikasi, dan meredistribusikan ini — termasuk secara komersial —
asalkan meneruskan kebebasan yang sama dan menyediakan sumbernya. Sumber lengkap ada di repo ini.

Komponen pihak ketiga:

- [JUCE](https://juce.com) — AGPLv3 / komersial, © Raw Material Software Limited
- [NeuralAmpModelerCore](https://github.com/sdatkinson/NeuralAmpModelerCore) — MIT, © Steven Atkinson
- [Eigen](https://eigen.tuxfamily.org) — MPL2
- [Steinberg ASIO SDK](https://www.steinberg.net/developers/) — hanya di build `MILODIKFX_ENABLE_ASIO=ON`;
  SDK tidak diredistribusi di repo ini.
- [Microsoft Edge WebView2](https://developer.microsoft.com/microsoft-edge/webview2/) — merender UI saat runtime.
