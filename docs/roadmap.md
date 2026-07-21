# MilodikFX Roadmap — Backlog Pasca-Audit

**Status:** Draft, diurutkan berdasarkan prioritas
**Dibuat:** 2026-07-21, hasil audit UI/UX + kualitas audio menyusul sesi rebuild native/ASIO
**Revisi:** 2026-07-21 sore — bagian UI ditambah temuan dari inspeksi **visual** jendela aplikasi yang berjalan (tangkapan via PrintWindow), bukan lagi hanya dari pembacaan kode
**Revisi 2:** 2026-07-21 malam — ditambah item adaptasi dari mockup desain `docs/E0B4AFA6-C41E-41B9-AFF3-4AAE87785844.png` (desain PerformView era v0.6). Mockup itu kini jadi **referensi desain resmi** untuk item UI: P1-5 (chain strip + kartu pedal), P0-2 (tampilan tuner), P0-3 (panel EXP/assign), P3-6b (meter LED tersegmen). Elemen mockup yang TIDAK diadaptasi: tombol **ADD BLOCK / CLEAR CHAIN** (pengubahan susunan chain saat runtime) — terhalang arsitektur `findProcessor<T>` yang mensyaratkan maksimal satu prosesor per tipe dan urutan chain yang tetap; mengubah itu berarti memberi ID per prosesor dan merombak registry/preset/REST sekaligus, effort besar dengan nilai kecil untuk pemakaian pribadi.
**Cara pakai dokumen ini:** setiap item independen — bisa dikerjakan satu-satu, dicentang, dan diminta implementasi kapan saja dengan menyebut nomornya (mis. "kerjakan P0-1"). Estimasi effort memakai skala weekend seperti di `CLAUDE.md`/riwayat kerja sebelumnya, bukan jam kerja penuh waktu.

Dasar arsitektur yang dipakai semua item DSP baru di bawah: `src/dsp/ChainFactory.cpp` adalah satu-satunya tempat yang perlu disentuh untuk mendaftarkan efek/parameter baru — begitu terdaftar di `ParameterRegistry`, otomatis muncul di REST API (`/api/effects`), UI (dibangkitkan dari situ), sistem preset, dan persistensi settings. Ini bukan pilihan, ini pola wajib supaya tidak terjadi drift seperti sebelum rombakan besar.

---

## Status implementasi

Diperbarui saat implementasi berjalan. Item yang sudah selesai tetap ditulis lengkap di bawah sebagai catatan keputusan desainnya, jangan dihapus.

**Selesai:**

- P0-1 IR Loader — `IrEngine`, `IrLibrary`, `IrHandler`, parameter teks di registry
- P0-2 Tuner — `TunerAnalyzer` (YIN di thread latar), `/api/tuner`, `TunerDisplay.tsx`
- P0-3 MIDI / Footswitch — `MidiController`, `/api/midi`, `MidiMapping.tsx` dengan MIDI Learn
- P1-1 Kurva EQ + metering SSE — `services/biquad.ts`, `ToneCurve.tsx`, `/api/levels/stream`
- P2-3 Scene — `SceneManager` (enabled-only), `/api/scenes`, `SceneGrid.tsx`
- P3-1 Metadata preset — deskripsi/tag/favorit/catatan, di luar `state`
- P3-7 Impor/ekspor preset — `/api/presets/export` + `/import`, unduhan Blob di browser
- P3-5 Undo/redo — `UndoHistory`, `/api/history`, tombol di top bar + Ctrl+Z / Ctrl+Shift+Z
- P3-6 Installer — Inno Setup terpasang, `MilodikFX-0.10.0-setup.exe` diverifikasi pasang → jalan → copot bersih
- P4-0 Input gain / trim — `InputTrimProcessor` di depan noise gate, `input.gainDb`, meter pasca-trim
- P4-2 Spillover ekor delay/reverb — fade hanya saat dimatikan, blok berhenti setelah ekor < −80 dB
- P4-1 Tipe overdrive — 8 voicing sebagai tabel data (`DriveVoicing.h`), kontrol UI menyesuaikan tipe
- P1-2 Overdrive asimetri + oversampling adjustable
- P1-3 Delay damping + ping-pong
- P1-4 Compressor parallel mix
- P1-5 Chain strip + kepadatan kartu
- P1-6 Global bypass (crossfade 10 ms)
- P2-2 Convolution reverb
- P2-4 Tap tempo + delay tempo-sync — BPM global dipakai bersama metronome dan delay
- P3-3 Pencarian preset
- P3-4 Global mute / panic (Esc = mute, B = bypass)
- P3-6a Title bar gelap (`DWMWA_USE_IMMERSIVE_DARK_MODE`)
- P3-6b Meter: skala berguna, peak hold, tampilan saat hening
- P3-8 Metronome — `MetronomeProcessor` sebagai post-processor, di luar jalur bypass
- P3-9 CPU sparkline

**Belum:** P2-1 (NAM), P2-5 (multi-view), dan seluruh backlog baru **P4** (tipe overdrive + adaptasi Fractal, ditambahkan 22 Jul 2026 — lihat bagian P4 di bawah).

**Kenapa dua itu belum:** NAM adalah dependensi eksternal dengan submodule, tanpa target CMake sendiri, dan belum pernah diuji di MSVC oleh upstream — riset teknisnya sudah lengkap di P2-1 di bawah, tapi integrasinya cukup besar untuk jadi satu sesi sendiri dan tidak pantas dikerjakan setengah jalan lalu ditandai selesai. Multi-view (P2-5) menunggu sampai jumlah panel di sidebar benar-benar terasa sesak; sekarang masih terbaca dalam satu layar.

**Rilis:** v0.10.0 — https://github.com/banumelody/MilodikFX/releases/tag/v0.10.0

**Catatan P3-5:** langkah undo dicatat setelah rantai diam ~700 ms, bukan setiap tulis. Memutar knob menghasilkan satu tulis per frame; kalau tiap tulis jadi satu langkah, Ctrl+Z lima puluh kali baru kembali ke awal. Ada unit test khusus untuk itu. Baseline dibaca ulang dari rantai setelah apply, karena parameter bisa membatasi nilai yang diberikan — baseline yang tidak cocok dengan rantai akan membuat commit berikutnya mencatat langkah yang tidak pernah dilakukan siapa pun.

**Keputusan desain P2-3 yang diambil:** scene menyimpan **hanya flag enabled**, sesuai rekomendasi awal. Alasannya terbukti saat implementasi — pergantian scene di tengah lagu tidak boleh melompatkan parameter ke nilai yang tidak terlihat pada kontrol yang tidak sedang disentuh. Ada unit test khusus untuk itu. Penyimpanan: di dalam preset (satu preset membawa empat scene-nya), plus salinan di settings supaya rantai kembali seperti ditinggalkan walau tidak ada preset yang dimuat.

**Catatan P1-1:** target 30 Hz, yang benar-benar terkirim ±22 Hz — Windows membulatkan sleep ke granularitas timer sistem (~15 ms). Tetap dua kali lipat polling 10 Hz yang lama dan cuma satu koneksi. Mengejar 8 Hz sisanya berarti menaikkan resolusi timer global demi sebuah meter, tidak sepadan.

**Catatan P0-3:** dispatch pesan MIDI diuji lewat `handleIncomingMidiMessage` langsung, bukan port fisik — mesin build tidak bisa diasumsikan punya controller. Yang belum pernah diuji dengan hardware sungguhan: apakah footswitch tertentu benar-benar mengirim 127/0 seperti dugaan. Kalau ada controller yang berperilaku lain, itu ketahuannya cuma saat dicoba.

### Catatan arsitektur yang lahir dari implementasi ini

`DSPChainManager` sekarang punya dua daftar prosesor: **chain** (yang dilewati sinyal gitar, kena global bypass) dan **post** (`addPostProcessor`, dijalankan setelah crossfade bypass). Metronome ada di daftar kedua — klik yang lewat rantai efek akan terdistorsi dan terfilter cabinet, dan akan ikut hilang saat bypass ditekan, justru ketika ketukannya paling dibutuhkan. `findProcessor<T>()` menyisir kedua daftar, jadi batas "maksimal satu prosesor per tipe" tetap berlaku menyeluruh.

Tuner **bukan** anggota chain sama sekali. `MainComponent` menyadap buffer input sebelum `audioEngine.processBlock`, karena pitch detection harus melihat sinyal pickup mentah — sinyal yang sudah lewat overdrive punya harmonik yang menipu YIN.

---

## P0 — Dikerjakan lebih dulu (dampak tinggi, effort kecil–menengah, fondasi sudah terverifikasi)

### P0-1. IR Loader untuk Cabinet

**Masalah:** `CabinetProcessor` sekarang adalah pendekatan analitik — 6 biquad berurutan (highpass 80 Hz, thump peak 105 Hz, scoop 400 Hz, presence peak 2600 Hz, lowpass ganda untuk roll-off). Ini cukup untuk menghilangkan fizz overdrive, tapi tidak akan pernah semirip cabinet asli yang di-mic.

**Solusi teknis:** `juce::dsp::Convolution` sudah tersedia di source JUCE 8 yang ter-vendor di repo ini (`build/_deps/juce-src/modules/juce_dsp/frequency/juce_Convolution.h` — sudah diverifikasi langsung, bukan asumsi). Kelas ini melakukan partitioned convolution di domain frekuensi, mendukung:
- Load IR dari file WAV atau memory saat runtime, dengan resampling & trimming otomatis
- Mode zero-latency (partitioned uniform) — cocok karena CPU kita masih sangat longgar (~1–2% terpakai saat ini)
- `loadImpulseResponse()` yang **wait-free**, aman dipanggil langsung dari audio thread tanpa perlu marshalling rumit seperti device manager

**Rencana implementasi:**
1. Buat `IrLoaderProcessor` baru (`src/dsp/IrLoaderProcessor.h/.cpp`), membungkus `juce::dsp::Convolution` + `juce::dsp::ConvolutionMessageQueue` (queue di-share, di-hold di `MainComponent` seperti `deviceController`).
2. Endpoint `/api/ir` (pola sama seperti `/api/presets`): `GET` daftar IR tersedia, `POST /api/ir/select` untuk memilih, upload file lewat multipart atau simpan manual ke folder.
3. Folder penyimpanan: `Documents\MilodikFX\ImpulseResponses\` — pola identik dengan `PresetManager`, termasuk sanitasi nama file (pelajaran dari bug traversal preset sebelumnya, jangan diulang).
4. Di `ChainFactory.cpp`: opsi *mode* pada Cabinet — `analytic` (default, perilaku sekarang tetap ada sebagai fallback aman) vs `ir` (pakai file yang dipilih). Kalau file IR belum dipilih atau gagal dimuat, otomatis jatuh ke analytic — jangan sampai user kehilangan suara karena file hilang/corrupt.
5. UI: dropdown pemilih IR di rack Cabinet (`EffectRack.tsx` sudah punya pola `ENUM_OPTIONS` untuk select — tinggal diperluas jadi dinamis dari daftar file, bukan hardcoded seperti `input.mode`).
6. Preset/settings: path IR yang dipilih ikut tersimpan (string, bukan angka — perlu sedikit perluasan `ParameterDescriptor` yang sekarang cuma `float`, atau disimpan sebagai field terpisah di luar registry seperti device state).

**Test yang wajib ada:**
- Unit test: load IR pendek (mis. 512 sample sintetis), verifikasi output tidak NaN/Inf, verifikasi energi output masuk akal dibanding energi IR.
- Unit test: fallback ke analytic saat path IR invalid/file tidak ada.
- Unit test regresi: prepareToPlay saat IR sedang dipakai (ganti sample rate) — mengikuti pola *fixed-allocation, no realloc under running audio* yang sudah jadi aturan wajib di semua prosesor sejak bug crash Delay kemarin.
- E2E: pilih IR lewat API, verifikasi `/api/effects/cabinet` melaporkan mode `ir` dan nama file yang aktif.

**Sumber IR untuk dicoba:** paket gratis dari OwnHammer, Celestion Impulse Responses, atau ML Sound Lab — semuanya WAV mono/stereo 48kHz umum dipakai, format yang didukung native oleh `Convolution::loadImpulseResponse`.

**Effort:** ±1 weekend. **Risiko:** rendah — building block sudah terverifikasi ada, pola foldernya sudah pernah dibangun untuk preset.

---

### P0-2. Tuner

**Masalah:** Belum ada tuner sama sekali. Versi lama (`TunerBridge` di `src/ipc/`) adalah kode mati yang sudah dihapus total di sesi sebelumnya — tidak ada yang bisa "dihidupkan lagi", ini fitur baru dari nol.

**Solusi teknis:** Deteksi pitch dari sinyal input mentah (sebelum efek lain memprosesnya — idealnya ambil sinyal langsung dari titik yang sama dengan level meter input, di `MainComponent::audioDeviceIOCallbackWithContext`, sebelum `audioEngine.processBlock`).

Algoritma yang cocok untuk gitar (fundamental 80 Hz–1200 Hz):
- **Autocorrelation** (ACF) sederhana, atau **YIN** (perbaikan autocorrelation, lebih tahan terhadap oktaf error) — YIN lebih disarankan, kompleksitas implementasi tidak jauh lebih tinggi dari ACF biasa dan hasilnya jauh lebih stabil untuk instrumen bersenar.
- Window ~1024–2048 sample @ 48kHz (≈21–43ms) — cukup untuk resolusi frekuensi rendah gitar (E2 ≈ 82 Hz butuh minimal ~2 periode penuh untuk deteksi andal).
- Tidak perlu FFT/library tambahan — YIN berbasis domain waktu murni, bisa dari nol tanpa dependency baru.

**Rencana implementasi:**
1. `src/dsp/TunerAnalyzer.h/.cpp` — bukan `AudioProcessorBase` biasa (tidak memproses/mengubah sinyal, hanya menganalisis), dipanggil terpisah dari chain utama, taruh di `MainComponent` seperti `levelsHandler`.
2. Buffer sirkular kecil untuk menampung window analisis, diisi dari audio callback, dianalisis di situ juga (bukan di thread lain) — cukup ringan, YIN pada 2048 sample jauh di bawah 1% CPU headroom yang ada sekarang.
3. Endpoint `/api/tuner` mengembalikan `{note, cents, frequency, confidence}` — pola sama dengan `/api/levels` (polling ringan, bukan realtime tinggi karena tuning tidak butuh update per-sample).
4. UI: komponen `TunerDisplay.tsx` baru — ikuti desain panel TUNER di mockup `docs/E0B4AFA6-...png`: huruf not besar di kotak, jarum deviasi -50..+50 cents, readout frekuensi Hz + cents di bawahnya, warna hijau saat in-tune (±5 cents).
5. Opsional: mode "mute saat tuning" — sinyal diam ke output selagi tuner aktif, kontrol lewat toggle di UI yang men-trigger `master.muted` sementara.

**Test:** unit test YIN terhadap sinyal sintetis sinus murni pada beberapa frekuensi standar (E2, A2, D3, G3, B3, E4), verifikasi hasil dalam toleransi ±2 cents; unit test terhadap sinyal ber-noise (simulasi pickup real) untuk verifikasi `confidence` turun secara wajar.

**Effort:** ±1 weekend (kecil, tanpa dependency baru). **Nilai pakai:** tinggi untuk pemakaian live sehari-hari.

---

### P0-3. MIDI / Footswitch Control

**Masalah:** Tidak ada cara mengontrol MilodikFX tanpa melihat layar — padahal modul `juce_audio_devices` yang sudah ter-link (lewat `juce_audio_utils`) memuat MIDI I/O lengkap (`juce_MidiDevices.h/.cpp`, terverifikasi ada), cuma belum pernah dipakai satu baris kode pun.

**Solusi teknis:**
1. `MainComponent` menambah `juce::MidiInputCallback` (atau kelas terpisah `MidiController` seperti pola `AudioDeviceController`), buka device MIDI default/pilihan lewat `juce::MidiInput::openDevice`.
2. Mapping CC (Control Change) → parameter registry: skema paling fleksibel adalah tabel mapping `{ccNumber -> {effectId, parameterId}}` yang bisa diatur user, disimpan di settings (`midi.cc.<n>.effect`, `midi.cc.<n>.parameter` mengikuti pola key `dsp.<effect>.<param>` yang sudah ada).
3. Program Change → pergantian preset (nomor program 0–127 dipetakan ke nama preset lewat urutan daftar atau mapping eksplisit).
4. Footswitch biasanya mengirim CC on/off (0/127) — perlu mode "momentary toggle" untuk parameter boolean (`enabled`, `muted`) supaya satu injakan = satu toggle, bukan harus ditahan.
5. Endpoint `/api/midi` untuk: daftar device MIDI tersedia, pilih device aktif, GET/PUT mapping CC.
6. UI: panel baru `MidiMapping.tsx` — mirip `DeviceSettings.tsx` (pilih device) ditambah tabel mapping yang bisa diedit, atau mode "MIDI Learn" (klik parameter di UI, gerakkan kontrol MIDI fisik, otomatis ter-mapping) — MIDI Learn jauh lebih ramah pengguna daripada isi tabel manual, disarankan sebagai pendekatan utama.
7. Untuk expression pedal kontinu, tampilkan sebagai panel ringkas gaya "EXP / ASSIGN" di mockup: daftar slot EXP dengan dropdown parameter tujuan (mis. EXP 1 → Overdrive Drive, EXP 2 → Delay Mix) — secara teknis ini mapping CC kontinu yang sama dengan butir 2, hanya penyajian UI-nya yang dibedakan dari footswitch on/off.

**Test:** unit test mapping table (CC → parameter, termasuk clamp ke range parameter seperti yang sudah jadi kontrak `ParameterRegistry::setParameter`); test manual dengan MIDI controller fisik atau virtual MIDI port (loopMIDI di Windows) untuk E2E — sulit diotomasi penuh tanpa hardware, jadi verifikasi manual jadi bagian wajar dari Definition of Done di sini.

**Effort:** ±1.5–2 weekend (device handling MIDI + UI mapping/learn mode lebih kompleks dari fitur lain di tier ini). **Nilai pakai:** mengubah alat ini dari "harus di depan laptop" jadi benar-benar bisa dipakai manggung dengan footswitch di lantai.

---

## P1 — Upgrade kualitas suara & UX yang terasa signifikan

### P1-1. Kurva EQ Visual + Upgrade Metering ke SSE

**Masalah gabungan** (dikerjakan bersamaan karena sama-sama butuh jalur data realtime baru dari backend ke frontend):
- EQ/Contour cuma angka, tidak ada gambar kurva respons frekuensi yang reaktif terhadap knob.
- Metering masih polling `GET /api/levels` tiap 100ms — cukup untuk VU-style, tapi kasar, dan menghabiskan satu koneksi HTTP baru tiap polling di server yang thread-per-connection.

**Solusi teknis:**
1. Ganti mekanisme level/meter dari polling ke **Server-Sent Events** (`text/event-stream`) — satu koneksi persisten, server push setiap kali level di-update dari audio callback (throttle ke ~30-60Hz, jauh lebih halus dari 10Hz polling sekarang tanpa membanjiri). `WebServer.cpp` perlu dukungan koneksi long-lived (server sudah pakai thread-per-connection, jadi SSE tinggal menahan koneksi terbuka dan menulis event berkala — perubahan model, bukan cuma tambahan endpoint).
2. Kurva EQ: dihitung di **frontend**, bukan backend — cukup kirim parameter band (freq, gain, Q — sudah ada semua di `ParameterDescriptor`) lewat `/api/effects` yang sudah ada, lalu frontend menghitung respons magnitude biquad (rumus RBJ standar, sama persis dengan `src/dsp/Biquad.h` tapi diporting ke TypeScript) dan menggambar sebagai SVG path di atas knob-knob EQ. Ini menghindari kebutuhan jalur data baru dari backend untuk kurva — parameter yang sudah ada cukup.
3. Reduction meter (compressor/limiter) yang sekarang sudah ada tinggal dialirkan lewat SSE yang sama, bukan pakai jalur terpisah.

**Test:** frontend unit test untuk fungsi hitung kurva biquad (bandingkan terhadap nilai referensi yang dihitung manual/Python untuk beberapa freq/gain/Q); E2E: buka koneksi SSE, verifikasi event masuk berkala saat audio berjalan.

**Effort:** ±1.5 weekend (perubahan model koneksi WebServer lebih berisiko daripada sekadar nambah endpoint — perlu hati-hati terhadap batas jumlah koneksi bersamaan dan graceful shutdown, mengikuti pola `stop()` yang sudah menunggu koneksi in-flight).

---

### P1-2. Overdrive: Asimetri + Oversampling Adjustable + Multi-stage

**Masalah:** `OverdriveProcessor` sekarang cubic soft-clip simetris tunggal, oversampling tetap 2×. Fungsional dan aman (sudah teruji di 41k+ assertion), tapi karakter tabung asli punya clipping asimetris (menghasilkan harmonik genap, bukan cuma ganjil) dan sering multi-stage (drive bertingkat, masing-masing dengan warna beda).

**Solusi teknis:**
1. Parameter baru `asymmetry` (0–1, default 0 = perilaku sekarang persis, backward compatible): offset DC kecil sebelum clipping, mengikuti pola klasik `x - offset` sebelum fungsi clip, lalu `+offset` sesudahnya untuk menjaga level rata-rata — teknik umum di amp sim untuk simulasi bias tabung.
2. Parameter `oversamplingFactor` enum (2×/4×/8×) — `juce::dsp::Oversampling` yang sudah dipakai sekarang mendukung factor lebih tinggi tinggal ganti angka konstruksi, tapi perlu diuji dampak CPU (di buffer 32 sample, 8× berarti proses di buffer virtual 256 sample — masih jauh dari mengkhawatirkan mengingat headroom CPU sekarang, tapi harus diukur, jangan diasumsikan).
3. Opsional lebih jauh (bisa ditunda ke item terpisah kalau dirasa kurang penting): dua tahap drive berurutan dengan kurva berbeda (mis. tahap 1 lembut/kompresif, tahap 2 lebih tajam) — menambah kompleksitas UI (parameter jadi dobel), pertimbangkan apakah ini benar-benar dipakai sebelum dibangun.

**Test:** unit test THD (total harmonic distortion) sederhana — masukkan sinus murni, FFT output, verifikasi rasio harmonik genap:ganjil berubah sesuai `asymmetry`; regression test bahwa `asymmetry=0` menghasilkan output identik dengan versi sebelum perubahan (byte-level match pada test sinyal yang sama).

**Effort:** ±1 weekend untuk asimetri + oversampling adjustable saja (disarankan tunda multi-stage sampai terbukti dibutuhkan).

---

### P1-3. Delay: Tone Filter di Feedback Path + Mode Ping-Pong

**Masalah:** `DelayProcessor` sekarang delay feedback bersih tanpa pewarnaan — setiap repeat identik dengan yang sebelumnya (selain redaman amplitude dari feedback%). Delay analog asli (tape/BBD) makin gelap setiap repeat karena bandwidth terbatas di jalur feedback.

**Solusi teknis:**
1. Tambah satu lowpass biquad (pakai `BiquadCoeffs`/`BiquadState` dari `src/dsp/Biquad.h` yang sudah ada) di dalam loop feedback `DelayProcessor::processBlock` — setiap kali sinyal melewati feedback, sedikit high-end hilang, mengakumulasi makin gelap tiap repeat secara alami. Parameter baru `dampingHz` (default tinggi/minim efek untuk backward compatible).
2. Mode ping-pong: alih-alih feedback ke channel yang sama, silangkan L→R dan R→L di jalur feedback (perlu buffer per-channel yang sudah ada, tinggal ubah logika indexing, bukan struktur data baru) — parameter `pingPong` boolean.
3. **Penting mengikuti pelajaran dari bug crash sebelumnya:** filter damping yang baru harus punya state (`BiquadState`) berukuran tetap per channel, dialokasikan sekali di konstruktor seperti `lines` di `DelayProcessor` sekarang — **tidak boleh** direalokasi di `prepareToPlay`, itu persis pola yang menyebabkan use-after-free kemarin.

**Test:** unit test bahwa `dampingHz` tinggi (~20kHz, efektif off) menghasilkan output identik dengan versi tanpa filter; unit test ping-pong memverifikasi sinyal impuls mono benar-benar berpindah channel di repeat kedua; regression concurrency test (pola sama seperti `ConcurrentPrepareTests` yang sudah ada di `RegistryTests.cpp`) untuk memastikan filter baru tidak reintroduce bug realokasi.

**Effort:** ±0.5–1 weekend — perubahan kecil di atas struktur yang sudah ada dan teruji.

---

### P1-4. Compressor: Parallel Mix (New York-style)

**Masalah:** Compressor sekarang cuma serial penuh (100% wet) — tidak ada cara mem-blend sinyal terkompresi dengan sinyal asli untuk teknik parallel compression yang populer di produksi audio (mempertahankan transient asli sambil menambah "beef" dari kompresi berat).

**Solusi teknis:** Tambah parameter `mixPct` (0–100%, default 100% = perilaku sekarang persis). Perlu simpan salinan sinyal dry sebelum `CompressorProcessor::processBlock` memproses (mengikuti pola yang sama seperti `ReverbProcessor` menyimpan `dry`/`wet1`/`wet2` — bukan konsep baru di codebase ini).

**Test:** unit test `mixPct=100` identik dengan perilaku sekarang; unit test `mixPct=0` menghasilkan output identik input asli (bypass efektif, tapi lewat compressor bukan lewat `enabled=false`).

**Effort:** kecil, ±0.5 weekend — murni penambahan satu parameter dan sedikit logika blend di prosesor yang sudah stabil.

---

### P1-5. Tata Letak Rack: Urutan Rantai Sinyal Harus Terlihat + Kepadatan Kartu

**Temuan visual (dari tangkapan jendela aplikasi berjalan, 1196×799):** Kartu efek tersusun sebagai grid 2 kolom yang membungkus ke bawah — secara visual **tidak ada petunjuk sama sekali bahwa ini rantai seri** (gate→boost→comp→drive→eq→contour→cab→delay→reverb→master). Tampilannya seperti halaman pengaturan, bukan pedalboard. Selain itu tinggi kartu tidak seragam: kartu Reverb punya baris kedua berisi satu knob (Mix) dengan ruang kosong besar di kanannya; kartu Master melebar dengan banyak ruang mati; total 11 kartu menuntut scroll panjang padahal kepadatan informasinya rendah.

**Referensi desain: mockup `docs/E0B4AFA6-...png`** — baris SIGNAL CHAIN-nya persis konsep yang dimaksud: kotak ikon per blok (IN → GATE → COMP → BOOST → OVERDRIVE → EQ → DELAY → REVERB → OUT) dengan warna aksen masing-masing, tersambung garis konektor, dari terminal input ke terminal output. Kartu efeknya juga contoh kepadatan yang dituju: lebar seragam gaya "pedal", header berwarna solid sesuai aksen + tombol power di pojok, knob dengan nilai di bawahnya, lampu ON di kaki kartu.

**Solusi yang diusulkan:**
- Baris "chain strip" horizontal di atas area rack mengikuti mockup: ikon/chip per efek sesuai urutan chain, aksen warna sama dengan kartunya, menyala sesuai enabled, bisa diklik untuk scroll ke kartunya (klik kanan/klik ganda = toggle enabled).
- Kartu dirapatkan ke gaya pedal mockup: lebar kolom seragam lebih sempit, header berwarna solid (bukan hanya titik kecil seperti sekarang), tombol power pindah ke header, lampu status ON di bawah — tinggi kartu jadi jauh lebih seragam karena knob tersusun kolom sempit.
- Yang TIDAK diikuti dari mockup: ADD BLOCK / CLEAR CHAIN (lihat catatan revisi 2 di atas — terhalang arsitektur chain tetap).

**Effort:** ±1 weekend. Tidak menyentuh backend sama sekali.

---

### P1-6. Global Bypass (Adaptasi Mockup — Dukungan Engine Sudah Ada)

**Temuan dari mockup + pemeriksaan kode:** Mockup punya toggle GLOBAL BYPASS di bar preset. Menariknya, **engine sudah mendukung ini sepenuhnya** — `DSPChainManager::setBypassed()/isBypassed()` ada, berfungsi, bahkan sudah teruji unit test ("Bypass keeps signal intact" di `UnitTests.cpp`), dan `PresetState` lama pun dulu punya field `globalBypass`. Kemampuan ini hilang dari permukaan saat rombakan REST: tidak pernah didaftarkan ke `ParameterRegistry`, jadi tidak ada di API, UI, maupun preset.

**Nilai pakai:** membandingkan cepat suara terproses vs bersih (A/B) tanpa mematikan efek satu-satu — berbeda dari Mute yang menghilangkan suara sama sekali.

**Solusi:** Daftarkan sebagai toggle pada efek virtual (mis. di entry `master` atau entry global baru) di `ChainFactory.cpp`/`MainComponent`, memanggil `audioEngine.setBypassed()`. Di UI: tombol toggle di topbar dekat status koneksi, gaya pill seperti mockup. Satu catatan teknis: bypass saat ini adalah hard switch — layak ditambah crossfade singkat (~10 ms) di `DSPChainManager::processBlock` supaya perpindahannya bebas klik, mengikuti disiplin smoothing yang sudah berlaku di semua parameter lain.

**Effort:** ±2–3 jam (tanpa crossfade) atau ±0.5 weekend (dengan crossfade + test).

---

## P2 — Investasi besar, dampak tinggi tapi butuh eksplorasi dulu

### P2-1. NAM (Neural Amp Modeler) Integration

**Masalah:** Overdrive sekarang adalah nonlinearitas matematis sederhana. NAM memungkinkan memuat model neural network yang benar-benar "menangkap" karakter ampli/pedal asli dari hasil reamping.

**Yang sudah pasti (bagian dari ekosistem NAM yang saya tahu secara umum):**
- Model didistribusikan sebagai file `.nam` (JSON berisi arsitektur + bobot terkuantisasi), umumnya arsitektur WaveNet-style dilated convolution (ada juga varian LSTM).
- Ada library C++ pendamping yang biasa dipakai untuk inferensi realtime di plugin/hardware pedal — dirancang khusus untuk use-case ini, bukan generic ML framework.
- Lisensi kode dan sebagian besar model bersifat permisif (MIT-style), cocok berdampingan dengan AGPLv3 proyek ini — **tidak** ada friksi lisensi seperti kasus ASIO kemarin.

**Yang BELUM saya verifikasi** (perlu riset lanjutan sebelum commit ke rencana implementasi detail — jangan mulai coding dari estimasi ini saja):
- API pasti library inferensinya (nama kelas, cara load model, cara panggil per-block).
- Dependency build yang dibutuhkan (kemungkinan Eigen atau tensor lib ringan custom — perlu dicek apakah menambah beban build/waktu compile signifikan seperti WebView2/JUCE FetchContent sekarang).
- Biaya CPU nyata per model di buffer 32 sample (0.67ms) — perlu prototype dan benchmark dengan model `.nam` sungguhan sebelum menjanjikan buffer sekecil sekarang tetap aman dipakai bersama NAM aktif.

**Rencana kerja (revisi bertahap, bukan langsung full implementasi):**
1. **Tahap riset (sendiri, sebelum coding):** fetch & baca source library inferensi NAM yang sebenarnya, konfirmasi API, konfirmasi dependency, coba compile contoh minimal di luar proyek ini dulu.
2. **Tahap prototipe:** integrasikan sebagai target build terpisah/eksperimental, load satu model `.nam` publik, ukur CPU di buffer 32/64/128 sample — kalau ternyata butuh buffer lebih besar dari yang dipakai sekarang untuk stabil, itu trade-off nyata yang perlu didiskusikan (masih jauh lebih baik dari histori device 480 smp/960 smp lama, tapi perlu transparan ke user).
3. **Tahap integrasi:** kalau prototipe sukses, baru dibuatkan `NamProcessor` mengikuti pola `ChainFactory`, endpoint `/api/nam` (pola sama seperti IR loader di P0-1 — realistis dua fitur ini berbagi banyak infrastruktur: folder model, endpoint upload, UI picker), taruh di posisi Overdrive (menggantikan atau berdampingan sebagai alternatif mode, mirip pola `analytic` vs `ir` yang diusulkan untuk Cabinet).
4. **Tahap validasi:** test dengan minimal 2-3 model `.nam` publik berbeda karakter (clean, crunch, high-gain) untuk memastikan tidak ada asumsi yang overfit ke satu model.

**Test:** unit test load/inferensi terhadap model referensi kecil dengan sinyal sintetis (verifikasi output masuk akal, tidak NaN, level wajar); benchmark CPU otomatis sebagai bagian test suite (assert waktu proses per block di bawah threshold tertentu, mengikuti semangat testing realtime yang sudah ketat di proyek ini).

**Effort:** ±3–5 weekend (termasuk tahap riset yang tidak bisa diestimasi presisi sebelum dilakukan). **Ini item paling tidak pasti di seluruh roadmap — effort bisa melar tergantung temuan di tahap riset.**

---

### P2-2. Convolution Reverb (IR Reverb) sebagai Mode Tambahan

**Masalah:** `ReverbProcessor` sekarang algoritmik (Freeverb/Schroeder klasik) — terdengar "reverb-y" tapi bukan ruang asli. Convolution IR reverb (hasil rekaman ruang/plate/spring asli) jauh lebih realistis.

**Solusi teknis:** Berbagi `juce::dsp::Convolution` yang sama dengan P0-1 (disarankan dibangun infrastruktur bersama: satu `IrLoaderProcessor` yang bisa dipakai untuk cabinet maupun reverb, dibedakan lewat kategori file/folder — `ImpulseResponses/Cabinets/` vs `ImpulseResponses/Reverbs/`). Reverb IR biasanya jauh lebih panjang dari cabinet IR (bisa beberapa detik), jadi CPU-nya perlu diukur terpisah — convolution cost scale dengan panjang IR.

**Keputusan desain yang perlu didiskusikan sebelum dibangun:** apakah menggantikan algoritmik reverb sepenuhnya, atau jadi mode alternatif (`algorithmic` vs `convolution`) seperti pola Cabinet. Menyimpan keduanya artinya dua algoritma reverb hidup berdampingan — lebih fleksibel tapi menambah kompleksitas UI (parameter Decay/Size/Width dari algoritmik tidak semuanya relevan untuk IR reverb yang panjang/karakternya tetap sesuai file).

**Effort:** ±1 weekend **kalau** dikerjakan setelah P0-1 (infrastruktur IR loader sudah ada, tinggal reuse). Berdiri sendiri tanpa P0-1: ±1.5–2 weekend.

**Dependency:** sebaiknya dikerjakan setelah P0-1 selesai dan terbukti stabil.

---

### P2-3. Sistem Scene (Adaptasi Mockup)

**Referensi mockup:** panel SCENE — 4 slot bernama (Clean/Crunch/Lead/Solo) dengan grid yang menunjukkan efek mana yang aktif per scene, berpindah lewat tombol 1–4. Di UI lama ini fiksi client-side murni (state React yang hilang saat refresh); yang diusulkan di sini adalah versi sungguhan.

**Konsep:** Scene = snapshot ringan yang bisa dipindah **instan** saat bermain — berbeda dari preset (yang lebih berat, tersimpan sebagai file, untuk pergantian antar-lagu). Fondasi teknisnya sudah ada dan teruji: `ParameterRegistry::captureState()/applyState()` (yang dipakai preset) bisa dipakai persis sama untuk scene.

**Keputusan desain yang perlu diambil sebelum implementasi** (jangan langsung koding):
- Isi scene: hanya flag `enabled` per efek (murni seperti grid mockup — pindah scene = pola on/off berubah, knob tetap), atau snapshot parameter penuh? Rekomendasi awal: **enabled-only** dulu — lebih sederhana, lebih bisa diprediksi saat live, dan pergantiannya bebas glitch karena semua prosesor sudah punya kelakuan enable/disable yang teruji; snapshot penuh menyusul kalau terbukti dibutuhkan.
- Penyimpanan: scene ikut tersimpan **di dalam preset** (satu preset membawa 4 scene-nya, seperti implikasi mockup) — berarti perluasan struktur file preset, selaras dengan item P3-1 (metadata).
- Pergantian via MIDI Program Change/footswitch → beririsan dengan P0-3; idealnya scene 1–4 bisa dipetakan ke 4 footswitch.

**Implementasi kasar:** endpoint `/api/scenes` (GET daftar + state aktif, POST pilih scene, PUT simpan state sekarang ke slot N); UI panel `SceneGrid.tsx` mengikuti mockup (baris per scene, kolom per efek, klik sel untuk toggle); nama scene bisa diedit.

**Test:** unit test bahwa apply scene enabled-only tidak menyentuh nilai parameter; E2E pindah scene → verifikasi `/api/effects` melaporkan pola enabled yang sesuai.

**Effort:** ±1–1.5 weekend setelah keputusan desain diambil.

---

### P2-4. Tap Tempo + Delay Tempo-Sync (Adaptasi Mockup)

**Referensi mockup:** tombol TAP / 120.0 BPM di kiri bawah, dan kartu Delay yang punya "Sync 1/4" — delay time dinyatakan sebagai pembagian ketukan, bukan milidetik mentah.

**Konsep:** BPM global (di-set lewat tombol tap atau angka), lalu Delay dapat mode sync: `timeMs` dihitung dari BPM × pembagian not (1/4 = 60000/BPM ms, 1/8, 1/8 bertitik, dst.). Karena `DelayProcessor.setTimeMs` sudah smoothed (glide ala tape), perpindahan tempo otomatis halus tanpa kerja tambahan.

**Implementasi kasar:** parameter global `bpm` (registry, efek virtual "global" — bisa satu rumah dengan Global Bypass P1-6); pada Delay tambah `syncMode` enum (Off/1/4/1/8/dotted-1/8/triplet) yang, saat bukan Off, membuat `timeMs` diturunkan dari BPM (UI menonaktifkan knob Time manual). Deteksi tap di frontend (rata-rata interval 2–4 ketukan terakhir) → PUT `bpm`.

**Effort:** ±1 weekend. **Sinergi:** fondasi BPM ini juga prasyarat metronome (P3-8).

---

### P2-5. Navigasi Multi-View: Perform / Edit / Library / Settings (Adaptasi Mockup)

**Referensi mockup:** tab PERFORM | EDIT | LIBRARY | SETTINGS di header. UI sekarang satu halaman — masih cukup, tapi akan sesak begitu tuner (P0-2), MIDI mapping (P0-3), dan scene (P2-3) menambah panel.

**Konsep pembagian:**
- **Perform** — layar live: chain strip, scene, tuner, meter besar, master; kontrol besar-besar, minim teks (inilah yang digambarkan mockup secara keseluruhan).
- **Edit** — layar detail: rack kartu efek lengkap seperti UI sekarang.
- **Library** — preset browser (menyerap P3-1/P3-3: metadata, pencarian, favorit).
- **Settings** — device audio, MIDI mapping, preferensi.

**Kapan dikerjakan:** **jangan sekarang** — baru bernilai setelah minimal tuner + scene ada (kalau dikerjakan lebih dulu, tab Perform isinya kosong). Ditaruh di P2 sebagai penanda arah, dengan dependency eksplisit.

**Implementasi kasar:** state router ringan di `App.tsx` (tanpa library routing — cukup `useState<View>` karena tidak butuh URL), topbar menjadi milik semua view, panel-panel yang ada dipindah ke view masing-masing.

**Effort:** ±1–1.5 weekend. **Dependency:** setelah P0-2, P2-3 (dan idealnya P0-3) selesai.

---

## P3 — Nice-to-have, housekeeping, dan polish

### P3-1. Kembalikan Metadata Preset (tags/category/description)

**Temuan:** Saat sistem preset ditulis ulang mengikuti `ParameterRegistry` (supaya otomatis mengikuti parameter apa pun yang terdaftar), struktur lama `PresetState` yang punya field `author`/`description`/`category`/`tags`/`createdAt`/`modifiedAt` ikut hilang. Preset sekarang cuma `{schemaVersion, name, savedAt, state}`. Bukan bug — semuanya masih berfungsi — tapi regresi fitur kalau kamu suka mengorganisir banyak preset.

**Solusi:** Tambah field opsional di level penyimpanan preset (`PresetManager::savePreset`), **di luar** `state` (yang murni hasil `ParameterRegistry::captureState()`) — supaya tidak mencampur metadata organisasi dengan data parameter DSP. UI: kolom deskripsi/tag saat menyimpan preset, filter/search di `PresetControls.tsx` berdasarkan tag. Dari mockup, dua field lagi masuk di sini: **favorit** (ikon bintang di bar preset) dan **catatan bebas** (panel NOTES di kiri bawah mockup — per preset, bukan global).

**Effort:** ±0.5 weekend.

---

### P3-2. Kejelasan EQ vs Contour

**Temuan:** Dua blok dengan konsep mirip (EQ: 120Hz/1kHz/7kHz shelf+peak+shelf; Contour: 50Hz/500Hz/5kHz tiga peaking) tanpa perbedaan fungsi yang jelas di UI — berpotensi membingungkan pengguna baru (termasuk kamu sendiri enam bulan dari sekarang).

**Solusi (pilih salah satu, perlu didiskusikan, bukan keputusan teknis murni):**
- (a) Perjelas deskripsi masing-masing di `ChainFactory.cpp` (field `description` sudah ada di `EffectDescriptor`, tinggal ditulis lebih informatif — "EQ: pembentukan nada sebelum drive" vs "Contour: pembentukan nada final sebelum cabinet") — effort minimal, murni copy.
- (b) Gabungkan jadi satu blok tone-stack dengan lebih banyak band — perubahan struktural, mempengaruhi urutan chain, preset lama, dan effort jauh lebih besar.

**Rekomendasi:** mulai dari (a), evaluasi setelah dipakai beberapa minggu apakah (b) benar-benar diperlukan.

**Effort:** ±1 jam untuk (a).

---

### P3-3. Preset Browser: Pencarian & Info Waktu Simpan

**Masalah:** Dropdown polos, tidak ada pencarian, dan `savedAt` yang sudah tersimpan di file JSON preset tidak ditampilkan sama sekali di UI.

**Solusi:** `PresetControls.tsx` — tambah kolom input pencarian yang memfilter daftar lokal (tidak perlu endpoint baru, data sudah ada dari `GET /api/presets`), tampilkan `savedAt` di list (butuh sedikit perluasan `PresetsResponse` untuk menyertakan timestamp per preset, bukan cuma nama).

**Effort:** ±0.5 weekend.

---

### P3-4. Global Mute / Panic Shortcut

**Masalah:** Kontrol mute terkubur di dalam rack Master, butuh scroll/klik untuk keadaan darurat (feedback, noise tiba-tiba).

**Solusi:** Keyboard shortcut global (mis. Space atau Esc, perlu dipilih yang tidak konflik dengan interaksi knob lain) yang langsung memanggil `PUT /api/parameters/master.muted` (sudah ada endpoint-nya dari P0 sebelumnya — murni penambahan event listener di `App.tsx` + tombol besar yang selalu terlihat di topbar, bukan di dalam rack).

**Effort:** ±0.5 weekend.

---

### P3-5. Undo/Redo Parameter

**Masalah:** Semua perubahan langsung tersimpan tanpa jaring pengaman — tidak ada cara "batalkan 5 perubahan terakhir" kalau salah putar knob saat live.

**Solusi:** Stack undo sederhana di frontend (state history dari perubahan `handleParameterChange`/`handleEnabledChange` di `App.tsx`), replay ke API saat undo dipanggil. Cukup di level UI, tidak perlu perubahan backend — backend sudah stateless terhadap "riwayat", cuma menyimpan nilai terkini.

**Effort:** ±1 weekend (perlu hati-hati terhadap interaksi dengan coalesced writes yang sudah ada — jangan sampai undo memicu banjir request seperti yang pernah dicegah).

---

### P3-6a. Title Bar Gelap (Temuan Visual)

**Temuan:** Title bar jendela masih abu-abu terang bawaan Windows — satu-satunya elemen terang di aplikasi yang seluruhnya bertema gelap; kontrasnya mencolok dan terasa belum selesai.

**Solusi:** Panggil `DwmSetWindowAttribute` dengan `DWMWA_USE_IMMERSIVE_DARK_MODE` (attribute 20) pada HWND jendela utama saat dibuat di `Main.cpp` — API resmi Windows 10 1809+, beberapa baris kode.

**Effort:** ±1 jam.

---

### P3-6b. Meter: Skala Berguna, Peak Hold, dan Tampilan Saat Hening (Temuan Visual)

**Temuan:** Meter input/output memakai rentang penuh -100..+6 dBFS, sehingga noise floor idle (-77 dB) sudah menggambar bar hijau ~20% — meter tampak "aktif" dalam keheningan, dan rentang yang benar-benar berguna untuk bermain (-30..0 dB) terkompresi di ujung kanan. Angka "-77.4 dB" juga terus berkedip saat hening — informasi bising tanpa nilai.

**Solusi:** Skala tampilan meter mulai dari sekitar -60 dB (floor -100 tetap dipakai internal, hanya pemetaan visual yang berubah); tampilkan "—" di bawah ambang senyap (mis. -65 dB) alih-alih angka noise floor; tambahkan peak-hold marker sederhana (garis tipis yang turun perlahan) — semuanya murni di `LevelMeter.tsx`.

**Effort:** ±0.5 weekend.

---

### P3-6c. Konsistensi Bahasa UI (Temuan Visual)

**Temuan:** UI mencampur dua bahasa: kerangka aplikasi berbahasa Indonesia ("Terhubung", "Optimalkan latensi", "Belum ada preset", "Audio berjalan") sementara nama efek, parameter, dan deskripsinya berbahasa Inggris ("Feedback delay with a gliding time control", "Output level and safety limiter", "Size/Decay/Width"). Fungsional, tapi terasa tidak selesai.

**Solusi:** Pilih satu (disarankan: label parameter tetap istilah teknis Inggris yang lazim di dunia audio — Drive, Ratio, Decay — tapi kalimat deskripsi efek dipindah ke Indonesia lewat field `description` di `ChainFactory.cpp`), lalu sisir seluruh string UI sekali jalan.

**Effort:** ±2 jam.

---

### P3-7. Preset Import / Export (Adaptasi Mockup)

**Referensi mockup:** tombol IMPORT / EXPORT di bar preset.

**Nilai pakai:** berbagi preset antar mesin / dengan teman (relevan sekarang karena exe sudah dirilis publik), dan backup.

**Solusi teknis:**
- **Import** — jalur mudah: file picker di browser (`<input type="file">`) membaca isi JSON preset di frontend, lalu POST isi itu sebagai body ke endpoint `/api/presets/import` yang memvalidasi (schemaVersion, struktur `state`) sebelum menulis lewat `PresetManager::savePreset`. Tidak butuh multipart — cukup body JSON biasa, konsisten dengan seluruh API yang ada.
- **Export** — ada ketidakpastian yang harus dicek dulu: perilaku unduhan file (`<a download>`) di dalam `WebBrowserComponent`/WebView2 milik JUCE belum diverifikasi (bisa jadi tidak memunculkan dialog simpan). Jalur aman yang pasti bekerja: endpoint `/api/presets/export` menulis salinan ke `Documents\MilodikFX\Exports\` lalu backend membuka folder itu di Explorer (`juce::File::revealToUser`) — sederhana dan tidak bergantung pada perilaku unduhan WebView.

**Effort:** ±0.5–1 weekend (termasuk verifikasi perilaku unduhan WebView2).

---

### P3-8. Metronome (Adaptasi Mockup)

**Referensi mockup:** panel METRONOME kiri bawah (tampil OFF di mockup).

**Catatan sejarah:** `MetronomeBridge` lama adalah kode mati yang tidak pernah berbunyi dan sudah dihapus — ini fitur dari nol, sama seperti tuner.

**Solusi teknis:** Generator klik sederhana (burst sinus/noise pendek ber-envelope, aksen di ketukan 1) yang **di-mix langsung di tahap MasterOut** — bukan lewat rantai efek (klik metronome tidak boleh kena overdrive/reverb). Tempo dari parameter `bpm` global — **dependency: P2-4** (fondasi BPM dibangun di sana). Volume klik sebagai parameter sendiri. Semua state metronome di audio-thread mengikuti disiplin atomic + alokasi-tetap yang berlaku.

**Test:** unit test bahwa klik muncul di output pada interval sampel yang tepat untuk BPM tertentu; verifikasi klik tidak melewati rantai efek (aktifkan overdrive ekstrem, klik harus tetap bersih).

**Effort:** ±0.5–1 weekend setelah P2-4.

---

### P3-9. CPU History Sparkline + Versi Aplikasi di Header (Adaptasi Mockup)

**Referensi mockup:** grafik CPU HISTORY kecil di kanan bawah, bar CPU mini di topbar, dan "v0.6.0" di bawah logo.

**Solusi:** Murni frontend untuk sparkline — data `cpuPercent` sudah di-poll tiap 100 ms, tinggal disimpan ke ring buffer client-side (mis. 60 detik terakhir) dan digambar sebagai polyline SVG kecil di panel Performa. Versi aplikasi: tampilkan di bawah logo topbar, ambil dari `/api/health` (perlu cek dulu endpoint itu sudah menyertakan versi atau belum — kalau belum, tambah satu properti dari `ProjectInfo::versionString`).

**Effort:** ±2–3 jam.

---

### P3-6. Selesaikan Installer (Inno Setup)

**Status:** `installer/MilodikFX.iss` sudah ditulis di sesi sebelumnya, tapi **belum pernah benar-benar dijalankan** karena `iscc.exe` (Inno Setup Compiler) belum terpasang di mesin development. `scripts/build-release.ps1` sudah menangani kasus ini dengan baik (skip installer secara graceful, tidak gagal), tapi hasil installer (`MilodikFX-x.x.x-setup.exe`) belum pernah divalidasi end-to-end.

**Solusi:** Pasang Inno Setup (`winget install JRSoftware.InnoSetup` atau unduh manual dari jrsoftware.org), jalankan `scripts/build-release.ps1` tanpa `-SkipInstaller`, verifikasi installer benar-benar memasang, membuat shortcut, dan uninstaller bekerja bersih.

**Effort:** ±1 jam (murni verifikasi, skripnya sudah ada).

---

## P4 — Backlog baru (22 Jul 2026): tipe overdrive & adaptasi Fractal

Latar: pertanyaan "butuh amp simulator tidak?" dan "fitur Fractal mana yang bisa diadaptasi?".

**Jawaban amp/head simulator, supaya keputusannya tercatat:** cabinet simulator **sudah ada** (analitik + IR loader). Yang belum ada adalah bagian *head*-nya — voicing preamp dan kompresi/sag power amp. Keputusan: **tidak membangun head sim analitik tersendiri.** Alasannya berlapis: (1) tipe drive "amp-in-a-box" di P4-1 menutup kebutuhan ringannya dengan usaha jauh lebih kecil; (2) NAM (P2-1, riset sudah lengkap) adalah jawaban sebenarnya untuk amp modeling — satu file profil menangkap preamp + power amp + interaksinya sekaligus, dengan kualitas yang tidak akan terkejar oleh model analitik buatan tangan; (3) jalur lengkapnya sudah tersusun: OD types → EQ/Contour → Cabinet IR untuk "pedal ke amp bersih", NAM → IR untuk "amp sungguhan". Kalau setelah AIAB terasa masih kurang "hidup", parameter sag/presence bisa ditambahkan ke tipe AIAB — bukan blok baru.

**Fitur Fractal yang TIDAK diadaptasi, dan alasannya:** X/Y channel per blok (scene sudah menutup kebutuhannya; kombinasi scene × channel per blok membingungkan untuk pemakaian satu orang), chain dinamis (alasan arsitektur di Revisi 2), dan global blocks (hanya berguna kalau preset berjumlah ratusan). Yang sudah ada duluan di sini: scene, tuner, metronome, tap tempo + sync, tempo global, bypass per blok via MIDI.

### P4-0. Input Gain / Trim — **kerjakan pertama di P4**

**Masalah:** tidak ada satu pun cara menyetel level sinyal yang masuk ke rantai. Yang ada sekarang: `input.mode` (routing kanal saja, tanpa gain), `cleanBoost` 0…+24 dB (**hanya bisa menaikkan**, dan letaknya *setelah* noise gate), dan `compressor.inputGainDb` (−24…+24 dB, tapi itu untuk mendorong threshold kompresornya sendiri, bukan seluruh rantai). Akibatnya, level pickup menentukan bunyi seluruh rantai dan tidak bisa dikoreksi: humbucker aktif (EMG bisa 6–10 dB lebih panas dari passive) membuat overdrive lebih kotor pada setelan Drive yang sama, sementara single coil vintage yang lemah membuat setelan yang sama terdengar hambar. Ganti gitar berarti mendial ulang seluruh rantai, bukan satu knob. Menaikkan gain di interface bukan jawaban — itu menggeser titik clipping converter dan tetap tidak bisa **menurunkan** sinyal yang sudah terlalu panas.

**Rekomendasi: prosesor baru `InputTrimProcessor`, jadi stage paling awal rantai**, dan parameternya digabung ke kartu **Input** yang sudah ada (`input.gainDb`, −24…+24 dB, step 0.1, default 0).

**Kenapa harus sebelum noise gate.** Ini keputusan yang menentukan. Kalau trim di depan gate, threshold gate tetap benar secara relatif terhadap sinyal: ganti gitar → setel ulang trim saja, gate ikut menyesuaikan. Kalau trim setelah gate, threshold gate terikat ke level mentah interface, jadi ganti gitar berarti mendial ulang trim **dan** gate. Konsekuensinya: `cleanBoost` (yang ada setelah gate) **tidak bisa** dipakai sebagai trim hanya dengan melebarkan rentangnya ke negatif — lihat alternatif yang ditolak di bawah.

**Kenapa prosesor bertipe baru, bukan `GainProcessor` kedua.** `DSPChainManager::findProcessor<T>()` adalah pindaian `dynamic_cast` yang mengembalikan instance **pertama** bertipe itu, jadi rantai hanya boleh memuat satu prosesor per tipe. Menambahkan `GainProcessor` kedua akan membuat `findProcessor<GainProcessor>()` mengembalikan trim, bukan `cleanBoost` — aturan yang sudah didokumentasikan di `CLAUDE.md` dan tidak boleh dilanggar diam-diam.

**Meter harus ikut, kalau tidak knobnya buta.** `inputPeak` diukur di `MainComponent` **sebelum** `audioEngine.processBlock`, jadi trim sebagai stage rantai tidak akan terlihat di meter sama sekali — artinya kamu mendial gain tanpa umpan balik apa pun. Untungnya tidak perlu pengukuran kedua: trim adalah gain murni, jadi level pasca-trim **persis** level pra-trim + nilai trim dalam dB. Tambahkan satu field ke payload `/api/levels` (mis. `chainInputLevel`) yang dihitung begitu. Meter input menampilkan angka pasca-trim (itulah yang benar-benar diterima rantai), sementara angka pra-trim tetap tersedia untuk melihat sinyal yang sudah terlalu panas dari interface — hal yang tidak bisa diperbaiki oleh trim digital berapa pun.

**Kejelasan tiga kontrol gain** (bersaudara dengan P3-2, EQ vs Contour): setelah ini ada Input Gain, Clean Boost, dan Compressor Input. Deskripsi masing-masing harus menyatakan tugasnya, bukan cuma namanya — Input Gain: *"samakan level gitar ini dengan rantai, setel sekali per gitar"*; Clean Boost: *"dorong front-end untuk solo"*; Compressor Input: *"seberapa keras sinyal menabrak threshold kompresor"*.

**Yang perlu ikut berubah:** `GuitarChain` dapat anggota `inputTrim`; `buildGuitarChain` menaruhnya paling depan; `registerChainParameters` membangun efek `input` ketika prosesor trim ada, dengan `mode` ditambahkan hanya bila `ChainExtras` menyediakannya (sekarang seluruh kartu `input` bergantung pada extras) — sehingga build plugin tetap dapat trim meski tidak dapat routing kanal; urutan rantai di `CLAUDE.md` diperbarui; test `ChainFactory` yang mengharapkan `getNumProcessors() == 10` jadi 11, dan hitungan efek plugin/app bertambah satu. `ChainStrip` sudah menyaring `input`, jadi strip tidak berubah — trim diwakili oleh terminal "IN".

**Alternatif yang dipertimbangkan dan ditolak:**
- *Melebarkan `cleanBoost` ke −24…+24 dB.* Perubahan satu baris, tapi letaknya setelah noise gate (merusak sifat gate-mengikuti-trim di atas) dan memaksa satu knob mengerjakan dua tugas yang disetel pada waktu berbeda — trim sekali per gitar, boost per lagu.
- *Menerapkan trim di pemetaan input `MainComponent`.* Menguntungkan meter dan tuner secara gratis (keduanya membaca sebelum `processBlock`), tapi hanya berlaku untuk aplikasi standalone — build plugin tidak akan punya trim sama sekali — dan memaksa smoothing ditulis tangan di dalam callback audio alih-alih memakai `SmoothedParam` lewat `AudioProcessorBase` seperti stage lain.

**Test:** 0 dB harus passthrough **bit-identik** (default tidak boleh mengubah sinyal sedikit pun); −6 dB memotong amplitudo setengah, +6 dB menggandakan; perubahan besar meluncur tanpa step (pola yang sama dengan test glide level overdrive); clamp rentang dan `isfinite`. Yang paling penting: **test urutan** — trim +12 dB dengan threshold gate tertentu harus berperilaku sama dengan trim 0 dan input 12 dB lebih panas, membuktikan gate mengikuti trim. Registry: efek `input` mengekspos `gainDb` di build plugin maupun app. Meter: level pasca-trim yang dilaporkan sama dengan pra-trim + trim dB. E2E: ubah trim dari UI, engine melaporkannya, preset round-trip.

**Effort:** ±0.5 weekend. **Kenapa pertama:** paling kecil di P4, tidak bergantung apa pun, dan memperbaiki segalanya di hilirnya sekaligus — termasuk membuat kalibrasi delapan tipe overdrive di P4-1 bisa dilakukan terhadap level input yang diketahui, bukan terhadap kebetulan level pickup yang sedang dipakai.

---

### P4-1. Tipe Overdrive (adaptasi Drive block Fractal)

**Konsep:** parameter `overdrive.type` (enum) memilih voicing; setiap tipe menentukan pre-EQ → kurva clipping → post-EQ/tone → kompensasi level, meniru sirkuit aslinya. Knob yang tampil **menyesuaikan tipe** — bukan satu set knob generik.

**Arsitektur (mengikuti pola yang sudah ada):**
- Engine mendaftarkan **gabungan** semua parameter (`drivePct`, `tonePct`, `levelPct`, `bassDb`, `trebleDb`, `voice`, `boostDb`, dst.) sekali di `ChainFactory.cpp` — registry statis, preset/settings otomatis ikut. Tabel voicing per tipe adalah data (`struct DriveVoicing`: freq HPF pre-clip, boost mid pre-clip, kurva klip + asimetri, rentang tone post, blend bersih, kompensasi output), bukan cabang kode per tipe.
- Frontend menampilkan subset knob per tipe lewat peta seperti `ENUM_OPTIONS`/`OVERRIDDEN_BY` yang sudah ada di `EffectRack.tsx`, termasuk label per tipe (Gain vs Drive vs Boost).
- **Kompatibilitas mundur:** tipe pertama = `Custom` — persis perilaku sekarang (drive/level/asymmetry/oversampling). Preset lama memuat tanpa berubah bunyi; ada null-test untuk itu.
- Aturan realtime tetap: koefisien dihitung ulang hanya saat parameter bergerak, semua atomics, oversampling berlaku per tipe.

**Tipe dan kontrolnya (harus setia ke aslinya):**

| Tipe | Knob | Voicing kunci |
|---|---|---|
| Custom | Drive, Level, Asymmetry, Oversampling | Perilaku 0.10.0, untuk preset lama |
| Tube Screamer (TS808/TS9) | Drive, Tone, Level | HPF ~720 Hz **sebelum** klip (mid-hump, bass tidak terdistorsi), soft clip simetris di feedback, Tone = LPF variabel ~1–5 kHz |
| Bluesbreaker (KoT) | Gain, Tone, Volume | Soft clip headroom tinggi (klip mulai lambat), gain rendah–menengah, EQ nyaris flat + sedikit treble; paling dinamis terhadap picking |
| Blues Driver (BD-2) | Gain, Tone, Level | Dua tahap gain diskrit, klip asimetris ringan, terang/glassy (lift > ~1.5 kHz), bass hampir penuh; cleanup mengikuti volume gitar |
| Transparent (Timmy/Klon) | Gain, Bass, Treble, Level | Bass = pemangkas **pre**-clip (CCW), Treble = pemangkas **post**-clip; blend sinyal bersih yang ikut naik dengan Gain (rahasia Klon); klip keras threshold rendah ala germanium |
| OCD / MOSFET | Drive, Tone, Level + saklar HP/LP | Klip MOSFET ke ground (lebih keras, tepi kasar), HPF lebih rendah dari TS (low lebih terbuka), gain sampai nyerempet distorsi; HP = low lebih banyak + lebih agresif |
| Dumble (Zendrive) | Gain, Voice, Tone, Level | Voice = pre-EQ fokus mid (geser corner HPF + boost mid sebelum klip), klip asimetris lembut bertingkat (sustain vokal), post-LPF gelap |
| Marshall-in-a-box | Gain, Bass, Mid, Treble, Level | Dua klip kaskade (preamp), tone stack pasif interaktif gaya Marshall **setelah** klip (scoop mid saat semua jam 12), lift presence ~4 kHz |
| Clean Boost (EP) | Boost (dB), saklar Bright | Saturasi sangat halus + lift treble & low-mid "EP magic". Tumpang tindih dengan blok CleanBoost yang ada — nilai tambahnya warna EP dan posisinya di slot OD |

**Urutan pengerjaan (prioritas):**
1. **Kerangka per-tipe + Custom + Tube Screamer + Clean Boost** — TS adalah sirkuit paling terdokumentasi (patokan kalibrasi), Clean Boost trivial tapi memvalidasi mekanisme knob-berubah-per-tipe ujung ke ujung.
2. **Bluesbreaker + Blues Driver** — pasangan blues (favoritmu), keduanya kurva klip baru tanpa mesin tambahan.
3. **Transparent** — butuh mesin clean blend, dipakai ulang tipe lain kelak.
4. **OCD** — kurva MOSFET + saklar mode (saklar per-tipe = mekanisme UI baru yang kecil).
5. **Dumble** — kontrol Voice (pre-EQ variabel).
6. **Marshall-in-a-box** — tone stack interaktif, paling berat; terakhir.

**Test:** null-test `Custom` terhadap keluaran 0.10.0; per tipe: verifikasi spektral karakter kuncinya (TS: umpan 100 Hz + 1 kHz → harmonik 100 Hz jauh lebih kecil daripada tipe OCD, karena bass dipangkas sebelum klip; BD: rasio harmonik genap/ganjil menunjukkan asimetri; Klon: pada gain rendah keluaran ≈ input); level default antar tipe selaras ±1 dB (ganti tipe tidak boleh melompat keras); semua `isfinite`; oversampling bekerja di setiap tipe. E2E: ganti tipe dari UI → knob yang tampil berubah, nilai tersimpan per parameter, preset round-trip.

**Effort:** ±2 weekend, dipecah tiga batch sesuai urutan di atas.

### P4-2. Spillover ekor Delay/Reverb (adaptasi paling bernilai dari Fractal)

**Masalah:** mematikan Delay/Reverb (langsung, via scene, atau footswitch) memotong ekornya seketika — `processBlock` early-return saat disabled. Di Fractal, ekor dibiarkan berbunyi (spillover); justru fitur inilah yang membuat pindah scene terdengar mulus di tengah lagu.

**Solusi:** disabled = berhenti **mengumpankan input**, tapi jalur basah tetap diproses sampai ekor meluruh (feedback tetap jalan, lalu berhenti total begitu ekor < −80 dB supaya CPU tidak terbuang untuk keheningan). Enable kembali harus instan. Berlaku ke Delay dan Reverb; toggle `spillover` per efek, default menyala.

**Test:** impuls → disable → ekor meluruh alami, bukan terpotong; setelah ekor habis, blok benar-benar idle; pindah scene saat delay berbunyi → tidak ada klik. **Effort:** ±0.5 weekend. Prioritas tinggi karena memperbaiki scene yang sudah ada.

### P4-3. Dual IR + blend di Cabinet (adaptasi Cab block)

Dua `IrEngine` di cabinet, knob Mix A/B. Dua IR yang diblend adalah trik produksi standar (mic dekat + jauh, dua speaker). Infrastruktur IR sudah ada semua. **Effort:** ±0.5–1 weekend.

### P4-4. Modifier: envelope follower / LFO → parameter (adaptasi Modifiers)

Sumber modulasi (envelope dari input, LFO sinkron tempo) yang bisa dirutekan ke parameter registry mana pun — auto-wah, tremolo via volume, filter sweep sinkron tempo. Kekuatan khas Fractal, tapi butuh jalur modulasi di thread audio yang menulis parameter per blok — desain dulu sebelum koding. **Effort:** ±1.5–2 weekend. Kerjakan setelah P4-1/P4-2 terbukti.

### P4-5. Looper sederhana

Satu slot rekam/overdub/undo di akhir chain (post-master, seperti metronome). Mandiri dan tidak menyentuh arsitektur lain, tapi bukan kebutuhan inti — paling akhir. **Effort:** ±1–1.5 weekend.

---

## Ringkasan Urutan

| # | Item | Tier | Effort | Ketergantungan |
|---|---|---|---|---|
| 1 | IR Loader Cabinet | P0-1 | ~1 weekend | — |
| 2 | Tuner (tampilan mengikuti mockup) | P0-2 | ~1 weekend | — |
| 3 | MIDI/Footswitch + panel EXP/assign | P0-3 | ~1.5–2 weekend | — |
| 4 | Kurva EQ + SSE Metering | P1-1 | ~1.5 weekend | — |
| 5 | Overdrive asimetri+oversampling | P1-2 | ~1 weekend | — |
| 6 | Delay damping+ping-pong | P1-3 | ~0.5–1 weekend | — |
| 7 | Compressor parallel mix | P1-4 | ~0.5 weekend | — |
| 8 | Chain-strip + kartu gaya pedal (mockup) | P1-5 | ~1 weekend | — |
| 9 | Global Bypass (engine sudah siap) | P1-6 | ~0.5 weekend | — |
| 10 | NAM integration | P2-1 | ~3–5 weekend | riset dulu |
| 11 | Convolution reverb | P2-2 | ~1–2 weekend | idealnya setelah P0-1 |
| 12 | Sistem Scene | P2-3 | ~1–1.5 weekend | keputusan desain dulu |
| 13 | Tap tempo + delay sync | P2-4 | ~1 weekend | — |
| 14 | Navigasi multi-view Perform/Edit/Library/Settings | P2-5 | ~1–1.5 weekend | setelah P0-2, P2-3 |
| 15 | Metadata preset (+favorit, +notes) | P3-1 | ~0.5 weekend | — |
| 16 | Kejelasan EQ vs Contour | P3-2 | ~1 jam | — |
| 17 | Preset browser search | P3-3 | ~0.5 weekend | — |
| 18 | Global mute shortcut | P3-4 | ~0.5 weekend | — |
| 19 | Undo/redo | P3-5 | ~1 weekend | — |
| 20 | Title bar gelap | P3-6a | ~1 jam | — |
| 21 | Meter: skala berguna + peak hold + gaya LED | P3-6b | ~0.5 weekend | — |
| 22 | Konsistensi bahasa UI | P3-6c | ~2 jam | — |
| 23 | Selesaikan installer | P3-6 | ~1 jam | — |
| 24 | Preset import/export | P3-7 | ~0.5–1 weekend | — |
| 25 | Metronome | P3-8 | ~0.5–1 weekend | setelah P2-4 |
| 26 | CPU sparkline + versi di header | P3-9 | ~2–3 jam | — |
| 27 | **Input gain / trim** | P4-0 | ~0.5 weekend | — |
| 28 | Spillover ekor delay/reverb | P4-2 | ~0.5 weekend | — |
| 29 | Tipe overdrive (8 tipe, 3 batch) | P4-1 | ~2 weekend | idealnya setelah P4-0 |
| 30 | Dual IR + blend di cabinet | P4-3 | ~0.5–1 weekend | — |
| 31 | Modifier (envelope/LFO → parameter) | P4-4 | ~1.5–2 weekend | desain dulu; setelah P4-1/P4-2 |
| 32 | Looper sederhana | P4-5 | ~1–1.5 weekend | — |

Total estimasi kalau semua dikerjakan: kira-kira 27–35 weekend, dengan catatan NAM adalah yang paling tidak pasti dan bisa melar jauh dari estimasi tergantung hasil tahap riset.

**Urutan dalam P4** (bukan urutan nomornya): P4-0 input gain dulu karena paling kecil dan memperbaiki segala yang di hilirnya; lalu P4-2 spillover karena kecil dan langsung memperbaiki fitur scene yang sudah ada; baru P4-1 tipe overdrive yang jadi pekerjaan besarnya; sisanya menyusul.

**Catatan adaptasi mockup:** dari seluruh elemen mockup `docs/E0B4AFA6-...png`, hanya satu yang sengaja tidak diadaptasi — ADD BLOCK / CLEAR CHAIN (chain dinamis), karena bertabrakan dengan arsitektur satu-prosesor-per-tipe dan urutan chain tetap; alasan lengkap di catatan Revisi 2 di atas. Sisanya tersebar: chain strip & kartu pedal → P1-5, Global Bypass → P1-6, tuner → P0-2, EXP/assign → P0-3, scene → P2-3, tap tempo & sync → P2-4, tab navigasi → P2-5, star/notes preset → P3-1, import/export → P3-7, metronome → P3-8, CPU history & versi → P3-9, meter LED tersegmen → P3-6b. Elemen mockup yang sudah ada di aplikasi sekarang (tidak perlu item): pemilihan device/sample rate/buffer, indikator AUDIO RUNNING, master volume + mute, readout CPU/kHz/samples.
