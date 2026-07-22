# MilodikFX

MilodikFX adalah prosesor multi-efek gitar/bass realtime untuk Windows, dibuat untuk rig satu orang.
Satu executable C++20/JUCE mandiri: menjalankan audio engine, menyajikan HTTP API loopback, dan
menampilkan UI React **di dalam jendelanya sendiri** lewat Edge WebView2 — tanpa tab browser, tanpa
proses UI terpisah. Bundle UI tertanam di dalam exe, jadi binernya berdiri sendiri.

Target kedua membangun rantai DSP yang sama sebagai **plugin VST3** plus wrapper JUCE Standalone.

## Unduh

Build siap pakai ada di [halaman Releases](https://github.com/banumelody/MilodikFX/releases):

- **`MilodikFX-x.x.x-setup.exe`** — installer biasa, paling mudah dibagikan.
- **`MilodikFX-x.x.x-portable.exe`** — exe tunggal tanpa dipasang; WASAPI (shared/exclusive/low-latency)
  dan DirectSound. Jalan di Windows mana pun.
- **`MilodikFX-x.x.x.exe`** — dengan dukungan ASIO, latensi paling rendah.

Taruh di folder mana saja lalu jalankan. UI terbuka di jendelanya sendiri (butuh WebView2, sudah ada di
Windows 10/11 modern).

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

Fitur lain: **tuner kromatik** (gitar & bass 5-senar, sampai low B ≈ 31 Hz), **kontrol MIDI/footswitch**
dengan MIDI Learn, **scene** 4 slot, **preset** dengan metadata + impor/ekspor, **undo/redo**, kurva
respons EQ, metering lewat Server-Sent Events, dan **pengecekan update otomatis** — aplikasi memeriksa
GitHub Releases saat dibuka dan memunculkan pemberitahuan bila ada versi baru.

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
