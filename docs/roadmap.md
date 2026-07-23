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
- P0-2 Tuner — `TunerAnalyzer` (YIN di thread latar), `/api/tuner`, `TunerDisplay.tsx`. **Diperluas 22 Jul 2026:** dukungan bass 5-senar (sampai low B ≈ 31 Hz) lewat dekimasi ke ~16 kHz sebelum YIN — sekaligus optimasi (mencari periode 31 Hz di 96 kHz langsung ~10x lebih mahal) dan membuat biayanya independen dari sample rate device.
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
- P4-1b Voicing Centaur/RAT/Big Muff — **SELESAI (22 Jul 2026)**, 3 voicing baru di tabel (total 11 pedal): `germanium`+`hardClip` di `ClipCurve`, `toneMode` scoop-mid tilt Big Muff, pre-emphasis + Filter terbalik RAT, blend Klon Centaur. Kontrol UI + label per-pedal, level dicocokkan lintas voicing. Diuji: backend + 154 test frontend.
- P5-1 Update check, sponsor link, credit, situs — **SELESAI (22 Jul 2026, v0.15.0)**. `/api/update` (`UpdateHandler`) membandingkan `MILODIKFX_VERSION` dengan tag rilis terbaru GitHub; banner pemberitahuan yang bisa ditutup (dismissal diingat per versi di localStorage). Perbandingan versi = fungsi murni `isNewerVersion` (diuji, termasuk jebakan `0.9` vs `0.10`). Footer aplikasi memuat credit **Banu Antoro / @banumelody** + tombol sponsor; tautan eksternal dibuka di browser sistem lewat `newWindowAttemptingToLoad`. Situs GitHub Pages di `docs/` (`.nojekyll`).
- P4-3 Dual IR + blend di cabinet — `irFileB` + `irBlend`, default 0 sehingga tidak mengubah apa pun
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

- P5-2..P5-5 Audit optimasi — **SELESAI (22 Jul 2026, v0.16.0)**. P5-2 memoisasi (Knob/Toggle/EffectRack/ChainStrip + panel samping di-`memo`, callback App di-`useCallback` supaya memo bertahan lintas frame meter 22 Hz). P5-3 tuner lewat SSE (`/api/tuner/stream`), menggantikan poll 60 ms yang membuka ~17 socket/detik. P5-4 `UpdateHandler` fetch di luar lock + payload SSE satu-baris (`jsonOkCompact`). P5-5 default oversampling per voicing saat memilih tipe dari dropdown (fuzz dapat headroom, clean boost tidak).

- P6-1 Channel A/B/C/D + P6-4 spillover antar-preset + P6-5 MIDI/scene→channel — **SELESAI (v0.17.0–v0.18.0)**. Channel per efek (`ChannelStore`), scene kini juga menyimpan+memanggil channel per efek, dan MIDI bisa memetakan CC ke scene/channel (footswitch panggil scene). Sisa FM9: P6-2 (modifier) dan P6-3 (perform view).

**Belum:** P4-5 (looper), P6-2 (modifier — didetailkan dari P4-4, satu-satunya item FM9 tersisa).

**Kenapa empat itu belum, per 22 Jul 2026:**

- **P2-1 NAM — SELESAI (22 Jul 2026).** Diimplementasi penuh sesuai [`docs/nam-plan.md`](nam-plan.md): `NamProcessor` (head, di antara Contour dan Cabinet), `NamResampler`, `NamLibrary`, `/api/nam`, kartu rack + panel model. Terukur di Release ASIO 96 kHz / 32 smp: semua efek + model A1-Standard sungguhan = **~29% DSP** (>3x headroom). Handoff model tiga-atomic dengan reaper (tidak meniru plugin resmi yang men-free di callback audio), resampler CatmullRom (WindowedSinc 201-tap terlalu mahal — 46%), latensi diukur bukan diperkirakan. Diuji: 1,55 jt assertion backend (termasuk stress handoff konkuren, latency, perf), 151 test frontend, 45 E2E.
- **P4-4 Modifier** — butuh jalur modulasi baru di thread audio yang menulis parameter per blok. Roadmap sendiri menuliskan "desain dulu sebelum koding", dan keputusan desainnya belum diambil.
- **P4-5 Looper** — mandiri dan tidak menyentuh arsitektur lain, tapi bukan kebutuhan inti; paling akhir sejak awal.
- **P2-5 Multi-view** — sidebar masih terbaca dalam satu layar, jadi tab Perform/Edit/Library/Settings belum menyelesaikan masalah nyata. Akan terasa perlu begitu panelnya bertambah lagi.

**Rilis terbaru:** v0.19.0 — https://github.com/banumelody/MilodikFX/releases/tag/v0.19.0

**Catatan P4-1 yang lahir dari implementasi:** tiga hal yang hanya ketahuan lewat pengukuran, bukan pembacaan kode. (1) Split butuh dua filter sungguhan; mengurangi salinan low-pass *tampak* setara tapi menyisakan selisih fasa yang lalu kena gain penuh clipper — Tube Screamer terukur mendistorsi bass lebih keras daripada drive full-range, persis terbalik. (2) Tahap kaskade harus membagi gain; dua tahap gain penuh mengotakkan sinyal, DC blocker menengahkannya, dan harmonik genap — alasan utama memilih voicing asimetris — hilang sama sekali. (3) Test harmoniknya sempat mengukur kebocoran spektralnya sendiri; di luar bin analisis, fundamental menyebar di sekitar −43 dB, satu orde dengan harmonik yang diukur, sehingga kurva simetris tampak punya harmonik genap sebanyak yang asimetris. Tepat di bin, kurva simetris terbaca 0,000000.

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

### P4-1b. Voicing tambahan: Centaur, RAT, Big Muff — SELESAI (22 Jul 2026)

> **Status: terkirim di v0.14.0.** Ketiga voicing ada di tabel (`drive.type` 9/10/11) dengan kurva `germanium`/`hardClip`, `toneMode` scoop-mid Big Muff, pre-emphasis + Filter terbalik RAT, dan blend Klon Centaur. Kartu **tidak** jadi di-relabel "Drive" (id `overdrive` dan label "Overdrive" tetap, deskripsinya saja yang menyebut fuzz/distorsi) — mengganti label yang sudah dipakai preset lebih berisiko daripada nilainya. Analisis asli di bawah dipertahankan sebagai catatan desain.

**Pertanyaan:** bisa tidak menambah klon Centaur, Pro Co RAT, dan Big Muff? **Bisa** — dan cocoknya sebagai **tiga voicing baru di blok Overdrive** (`drive.type` 9/10/11), bukan stage chain baru. Alasannya berlapis: aturan satu-prosesor-per-tipe `findProcessor<T>` menutup opsi stage terpisah tanpa perombakan; posisi blok Overdrive di rantai (setelah kompresor, sebelum EQ/amp/NAM) persis tempat ketiga pedal ini di pedalboard nyata — fuzz → amp bersih (NAM) → cab adalah rig Big Muff klasik, dan urutannya sudah benar; dan tabel `DriveVoicing` memang dibangun supaya menambah pedal = menambah baris data. Enum-nya append-only, jadi preset lama aman.

Ketiganya genre berbeda (transparent OD / distortion / fuzz) tapi secara sinyal bentuknya sama: pre-EQ → nonlinearitas → post-EQ — persis yang dimodelkan tabel. Karena RAT dan Muff bukan "overdrive", kartu-nya layak di-relabel **"Overdrive" → "Drive"** sekalian (murah, hanya label; id `overdrive` tetap demi preset/settings).

**Apa yang SUDAH ada di tabel** (tidak perlu diubah):
- **Trik Klon** — `cleanBlend * amount` (blend bersih naik dengan gain, meniru pot dual-gang Centaur) sudah terpasang dan dipakai tipe Transparent. Komentarnya di kode bahkan bernama "The Klon trick".
- **Kaskade 2 tahap** dengan pembagian gain `sqrt` (dipakai MIAB/Blues Driver) — persis dua tahap clipping Big Muff.
- DC blocker, level-matching, crossfade drive-nol, oversampling per tipe.

**Ekstensi engine yang dibutuhkan** (data di tabel, bukan cabang per pedal — doktrin yang sama):

1. **Dua kurva klip baru** di `ClipCurve`:
   - `germanium` (Centaur): hard clip ambang rendah dengan lutut membulat — mulai terdistorsi lebih awal tapi halus; beda karakter dari `hardKnee` (MOSFET) yang dipakai Transparent.
   - `hardClip` (RAT): dioda silikon ke ground setelah op-amp — lebih keras dari `hardKnee`, sumber karakter "serak" RAT.
2. **Tiga field tabel baru**:
   - `preEmphasisHz/Db` (RAT): jaringan gain LM308 menguatkan mid/treble **sebelum** klip; tabel sekarang hanya punya presence **setelah** klip. Tanpa ini RAT cuma jadi "OCD dengan gain lebih".
   - `toneMode` (`lpSweep` | `midScoopTilt`) (Big Muff): tone Muff bukan sweep LPF tapi **tilt scoop-mid** — campuran LPF ~1 kHz + HPF ~1 kHz dengan notch dalam di jam 12. `VoiceState` sudah punya biquad state cadangan (bass/treble hanya dipakai MIAB/Transparent), jadi sisi HPF tilt bisa memakai state yang ada — tidak ada anggota baru, tidak ada alokasi baru.
   - `toneReversed` (RAT): knob Filter RAT terbalik — CW = makin gelap. Satu bool, arah sweep dibalik saat membangun koefisien.

**Spesifikasi per voicing:**

| Voicing | Knob (nama asli) | Kunci karakter |
|---|---|---|
| **Centaur** | Gain, Treble, Output | Kurva `germanium`; blend bersih naik dengan Gain (mekanisme yang sudah ada); split rendah (~100 Hz) sehingga low tetap di jalur bersih; Treble shelf post. Low gain ≈ boost bersih berkilau; high gain tetap "transparan" karena blend. |
| **RAT** | Distortion, Filter (terbalik), Volume | `driveMax` ~120 (terbesar di tabel); pre-emphasis high-shelf sebelum `hardClip`; Filter = LPF post arah terbalik. Di Distortion penuh menyerempet fuzz — memang begitu aslinya. |
| **Big Muff** | Sustain, Tone, Volume | Dua tahap kaskade gain besar (`stages=2`); Tone = `midScoopTilt` (notch mid di jam 12); split sangat rendah/0 sehingga low end besar ikut terdistorsi — ciri fuzz. |

**Catatan kejujuran soal Centaur:** tipe **Transparent yang ada memang sudah "Timmy/Klon-style"** — tumpang tindihnya nyata. Keputusannya: Transparent **tetap** (preset lama bergantung padanya; karakternya condong Timmy dengan Bass/Treble cut), Centaur ditambah sebagai tipe tersendiri dengan kurva germanium dan hanya 3 knob seperti aslinya. Dua entri yang mirip lebih jujur daripada mengubah bunyi tipe yang sudah dipakai preset.

**Aliasing:** hard clip dan fuzz menghasilkan harmonik tinggi jauh lebih banyak dari soft clip — ketiganya paling rentan aliasing di antara semua voicing. Default oversampling 2x yang ada sudah menolong; sarankan 4x di deskripsi. Test performa yang ada ("tidak ada voicing boleh 3x lebih mahal dari yang lain") otomatis mencakup tipe baru karena loop-nya `type < numTypes`.

**Test** (pola spektral/linearitas yang sama dengan P4-1, semua di bin analisis) — yang benar-benar dikirim di `tests/DriveVoicingTests.cpp`:
- Centaur: gain rendah ≈ bersih (rasio harmonik-3 kecil) tapi gain tinggi jelas terdistorsi; **lolos**.
- RAT: **hard distortion, bukan overdrive ringan** — pada setelan knob rendah rasio harmonik-3 RAT >3x clean boost (gain besar + `hardClip`); Filter CW terukur menggelapkan (test terbalik). *Catatan kejujuran:* rencana awal "harmonik RAT >> Tube Screamer" dan "pre-emphasis nada mid > nada rendah" **dibatalkan** — keduanya ternyata metrik rapuh: pada drive apa pun yang cukup untuk diukur, kedua kurva sama-sama mengotak jadi gelombang persegi (cubic TS mengunci tepat di ±1, sama kerasnya), dan tone LP + boost fundamental oleh pre-emphasis mengacaukan rasio harmonik per-frekuensi. Perbandingan vs clean boost menangkap "ini distorsi keras" secara tak ambigu; pre-emphasis tetap terpasang di sinyal, hanya tidak di-assert lewat sweep frekuensi.
- Big Muff: notch mid terukur di Tone jam 12 (magnitudo 1 kHz jauh di bawah 250 Hz dan 4 kHz); "sustain" = kompresi terukur (fundamental loud/quiet << 3,0 — kebalikan test split TS); level dicocokkan (`outputDb` dinaikkan ke +4 agar tidak melompat saat ganti voicing).
- Semua: test level-match, finite, oversampling, dan crossfade drive-nol yang ada otomatis memuat 3 tipe baru. E2E yang meng-assert `type.max == 8` diubah ke 11; `DRIVE_CONTROLS` + `ENUM_OPTIONS` di UI ditambah tiga entri.

**Urutan pengerjaan:** Centaur dulu (hanya butuh kurva baru — terkecil), lalu RAT (pre-emphasis + toneReversed + hardClip), lalu Big Muff (toneMode tilt — mekanisme terakhir). **Effort:** ±1–1,5 hari total. Tidak bergantung apa pun; bisa dikerjakan kapan saja.

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

## P5-2 – P5-5. Audit optimasi (22 Jul 2026)

Audit pembacaan kode + angka yang sudah terukur, bukan profiling baru — setiap item di bawah menyebut
bukti konkretnya. Kesimpulan besarnya dulu, supaya jujur: **jalur audio sehat dan tidak ada temuan
merah.** 7,6% DSP dengan semua efek menyala (16,3% worst case), tidak ada alokasi di `processBlock`,
koefisien di-cache dan hanya dihitung saat nilai bergerak, spillover punya early-out, handoff NAM
atomic dengan reaper di luar callback. Yang bisa dioptimasi ada di **sisi UI dan HTTP**, bukan DSP.

### P5-2. Memoisasi frontend — pemborosan render terbesar

**Temuan:** tidak ada satu pun `React.memo` di `frontend/src/components/` (diverifikasi grep). Stream
meter SSE mengalir ~22 Hz dan setiap frame memanggil `setLevels` di `App` — yang berarti **seluruh
pohon komponen di-render ulang 22 kali per detik**: 12 kartu `EffectRack`, puluhan `Knob` (masing-masing
menghitung busur SVG), panel samping, semuanya — padahal yang berubah hanya empat meter, sparkline, dan
empat angka statistik. Ini kerja render kontinu di proses WebView2 yang menyaingi drag knob dan memakan
CPU/baterai tanpa hasil visual.

**Rencana:** (1) `React.memo` untuk `EffectRack`, `Knob`, `Toggle`, dan panel samping yang prop-nya
stabil; (2) pindahkan konsumen `levels` (meter, stats, `Sparkline`) ke subtree terpisah sehingga frame
meter hanya me-render subtree itu; (3) `cpuHistory` berhenti menyalin array 600 elemen per frame —
pakai buffer terbatas. Ukur sebelum/sesudah lewat DevTools performance supaya klaimnya bukan perasaan.
**Effort:** 2–4 jam. **Risiko:** rendah; test yang ada tetap berlaku.

### P5-3. Tuner lewat SSE, bukan polling 60 ms

**Temuan:** `subscribeTuner` di `services/api.ts` mem-poll `/api/tuner` tiap 60 ms. Server adalah
thread-per-connection dengan `Connection: close` (diverifikasi di `WebServer.cpp`) — jadi selama panel
tuner terbuka, UI membuat **~17 koneksi TCP + thread baru per detik**. Mekanisme yang lebih tepat sudah
ada dan terbukti: `registerEventStream` yang dipakai `/api/levels/stream`.

**Rencana:** tambah `/api/tuner/stream` (interval ~60 ms, hanya mengirim saat tuner aktif), frontend
memakai `EventSource` dengan fallback polling seperti meter. Satu koneksi menggantikan ribuan.
**Effort:** 2–3 jam.

### P5-4. UpdateHandler: fetch di luar lock + SSE satu-baris

**Temuan (koreksi atas kode yang baru masuk di v0.15.0):** `UpdateHandler::handleGet` menjalankan fetch
GitHub **di dalam** `std::mutex` — permintaan `/api/update` kedua yang datang saat GitHub lambat ikut
tertahan; dan `withConnectionTimeoutMs(5000)` hanya membatasi *connect*, pembacaan stream yang macet
bisa menggantung lebih lama. Loopback-only dan jarang dipanggil, jadi dampaknya kecil — tapi polanya
salah dan murah diperbaiki.

**Rencana:** cek cache di dalam lock → fetch di luar lock → tulis hasil kembali di dalam lock; beri
batas waktu keseluruhan pada pembacaan. Sekalian: stream meter mengirim JSON pretty-print multi-baris
(`JSON::toString` default) yang lalu dipecah per baris untuk prefiks `data:` — kirim satu baris saja,
byte lebih kecil dan tanpa pemecahan. **Effort:** 1–2 jam.

### P5-5. Oversampling default per voicing (opsional, kualitas-vs-biaya)

**Temuan:** worst case DSP (16,3%) adalah Marshall-in-a-Box pada oversampling 8x. Voicing hard-clip/fuzz
(RAT, Big Muff) menghasilkan harmonik tinggi paling banyak dan paling butuh oversampling; clean boost
hampir tidak butuh sama sekali. Sekarang default oversampling seragam (2x) untuk semua tipe.

**Rencana:** tabel `DriveVoicing` diberi kolom oversampling *yang disarankan* per pedal, dipakai hanya
sebagai default saat user memilih tipe — pilihan manual user selalu menang. Fuzz mendapat kualitas
tanpa memaksa 8x ke voicing yang tidak membutuhkannya. **Effort:** 2–3 jam termasuk test.

---

## P6. Studi Fractal FM9 — apa itu, dan apa yang layak diadaptasi (analisis 22 Jul 2026)

### Apa itu FM9

**Fractal Audio FM9** adalah amp modeler + multi-efek format floor (satu keluarga dengan Axe-Fx III dan
FM3) yang dipakai luas oleh gitaris profesional untuk menggantikan seluruh rig amp + pedalboard. Kualitas
pemodelan amp-nya terkenal, tapi untuk MilodikFX bukan itu pelajaran utamanya — wilayah suara sudah
ditutup oleh NAM (capture amp) + IR cabinet. Yang menjadikan FM9 tolok ukur adalah **arsitektur
kontrolnya**: bagaimana satu unit mengelola banyak suara dalam satu lagu, berpindah di antaranya tanpa
jeda, dan tetap bisa dikendalikan kaki/tangan di panggung. Itu persis wilayah yang MilodikFX baru
setengah jalan.

### Konsep inti FM9, satu per satu

1. **Grid routing bebas.** Blok efek ditempatkan bebas di grid besar (6 baris), bisa seri, paralel,
   split/merge — dua amp berdampingan, delay paralel dengan reverb, dan seterusnya.
2. **Hierarki tiga tingkat: Preset ⊃ Scene ⊃ Channel.** Ini ide terpenting FM9.
   - **Preset** = seluruh rig: susunan grid + semua setelan. Ganti preset = muat rig baru.
   - **Scene** (8 per preset) = kombinasi *bypass* semua blok + *pilihan channel* tiap blok + level
     per scene. Ganti scene **instan tanpa loading** — semua blok sudah di memori — dengan spillover.
     Satu preset dengan 8 scene biasanya menutup satu lagu penuh: intro bersih, verse crunch, chorus
     high-gain, solo dengan delay panjang.
   - **Channel** (A–D per blok) = empat setelan tersimpan untuk blok yang sama. Satu blok drive
     menyimpan Tube Screamer gain rendah di A dan gain tinggi di B; scene memilih channel-nya. Tanpa
     channel, dua suara drive butuh dua blok; dengan channel, satu blok melayani empat suara.
3. **Modifier.** Hampir semua parameter bisa digerakkan sumber modulasi — LFO, envelope follower,
   pedal ekspresi, sequencer — lewat mapping min-max dengan kurva. Wah tanpa blok wah: LPF yang
   frekuensinya diikat ke pedal.
4. **Spillover antar scene dan antar preset.** Ekor delay/reverb tetap meluruh melewati pergantian.
5. **Perform pages.** Kumpulan kontrol pilihan user sendiri dalam satu layar — dari seluruh parameter,
   hanya 5–10 yang benar-benar disentuh saat manggung; FM9 membiarkan user memilih itu dan menaruhnya
   besar-besar di satu halaman.
6. **Layout footswitch fleksibel** — tiap switch bisa diberi fungsi apa pun per preset.

### Yang sudah setara di MilodikFX

Scene 4 slot (enable-only), spillover per efek antar scene, tempo global + delay sync + tap, tuner,
metronome, MIDI CC→parameter + PC→preset dengan MIDI Learn, preset dengan metadata. Fondasi hierarkinya
sudah ada — yang belum adalah tingkat ketiganya (channel) dan postur panggungnya (perform view).

### Yang sengaja TIDAK diadaptasi, dan kenapa

- **Grid routing bebas** — bertabrakan frontal dengan arsitektur yang disengaja: `findProcessor<T>`
  satu-prosesor-per-tipe, chain tetap, UI dibangkitkan dari registry. Ini keputusan yang sama dengan
  penolakan ADD BLOCK/CLEAR CHAIN di catatan mockup (Revisi 2 di atas). Untuk rig satu orang, nilai
  routing bebas kecil; biayanya perombakan registry/preset/REST total.
- **Global blocks** (setelan blok dibagi antar preset) — preset MilodikFX kecil dan murah diduplikasi;
  kompleksitas sinkronisasinya tidak sepadan.
- **Dual amp paralel** — satu NAM sudah memakan ~20% budget; dua berarti setengah budget untuk fitur
  yang jarang dipakai satu orang.
- **Layout footswitch hardware** — tidak ada hardware switch; padanannya keyboard + MIDI (P6-5).

### P6-1. Channel A/B/C/D per efek — jantung adaptasinya

> **Status: inti SELESAI di v0.17.0.** `ChannelStore` (`src/preset/ChannelStore.*`) menyimpan empat sound bernama A/B/C/D per efek; berpindah channel menyimpan sound yang aktif dulu lalu memuat yang dipilih (terasa live seperti FM9, tanpa mencegat tiap putaran knob). Persisten di preset (schema v4) **dan** settings file. API: `PUT /api/effects/<id>/channel {value}` (pindah) + `POST /api/effects/<id>/channel/save {value}` (simpan); `/api/effects` kini membawa `channel` (indeks aktif) + `channels` (empat nama) per efek. UI: tab A/B/C/D di tiap kartu efek yang punya bypass. Diuji: unit `ChannelStore` (auto-save-on-switch, round-trip JSON, edge case), 4 test frontend, 2 E2E (pindah channel + round-trip preset). **Yang belum:** tautan scene→channel (scene menyimpan indeks channel per efek dan memanggilnya saat recall) — dikerjakan bersama P6-5 karena keduanya soal memanggil channel dari kaki/otomasi. Analisis desain asli tetap di bawah.



Setiap efek mendapat hingga **4 set parameter bernama** (A/B/C/D), disimpan di dalam preset; scene
menyimpan *enable + pilihan channel* per efek. Satu preset lalu bisa memuat: scene 1 = drive A (crunch
tipis) + delay A (slap pendek); scene 3 = drive B (gain penuh) + delay B (dotted-eighth panjang) —
persis pola FM9, tanpa menggandakan efek.

**Ini merevisi prinsip "scene hanya menyimpan enable flags" — dan revisinya jujur.** Keberatan asli
(tercatat di P2-3): scene tidak boleh melompatkan parameter "ke nilai yang tidak kamu lihat, pada
kontrol yang tidak kamu sentuh". Channel menjawab keberatan itu, bukan menabraknya: lompatan hanya
menuju **keadaan bernama yang user buat sendiri**, ditampilkan terang-terangan di kartu (badge A/B/C/D),
dan scene tetap tidak menyimpan nilai parameter bebas — hanya *indeks* channel. Yang tak terlihat tetap
tidak ada.

**Desain teknis:**
- Penyimpanan: preset schema v4 — `channels: { <effectId>: [ {name, params:{...}}, ... ] }` +
  `activeChannel` per efek; scene mendapat `channel: { <effectId>: 0..3 }`. File schema 3 tetap
  termuat (efek tanpa channels = satu channel implisit).
- Realtime: ganti channel = serangkaian penulisan lewat setter registry yang sudah ada — semua
  parameter adalah atomic yang di-smooth, jadi ini setepat aman dengan MIDI CC yang menulis parameter
  hari ini. Tidak ada alokasi; set channel tersimpan di sisi non-audio.
- API: `PUT /api/effects/<id>/channel {index}`, `POST /api/effects/<id>/channel/save`; `/api/effects`
  memuat `channel` + daftar nama channel.
- UI: tab A/B/C/D kecil di header kartu; klik = pindah (menulis semua parameter kartu itu), klik-tahan/
  tombol simpan = simpan keadaan sekarang ke channel itu; scene grid menampilkan huruf channel tiap
  efek yang punya >1 channel.
- Spillover delay/reverb tetap bekerja — ganti channel mengubah parameter, bukan mematikan efek.

**Effort:** ~1.5–2 weekend. **Ketergantungan:** tidak ada; sebaiknya sebelum P6-3.

### P6-2. Modifier — mendetailkan P4-4 dengan model FM9

P4-4 selama ini berhenti di "desain dulu". Ini desainnya, meniru bentuk FM9:

- **Sumber** (tahap awal): LFO (sine/triangle/square, rate bebas atau sync tempo), envelope follower
  (mengikuti dinamika input chain), dan pedal ekspresi (MIDI CC yang sudah masuk lewat MidiController).
- **Mapping:** sumber menghasilkan 0..1 → dipetakan ke min..max dalam satuan parameter, dengan kurva
  (linear/log/exp) dan depth.
- **Aturan realtime yang menghindari perang tulis:** modifier TIDAK menulis parameter user. Ia
  mengevaluasi **per blok audio** (di 32 sampel = tiap 0,33 ms, jauh lebih halus dari yang terdengar)
  dan menghasilkan *offset modulasi*; prosesor memakai `efektif = clamp(base + depth × mod)`. Nilai
  base milik user tidak pernah berubah — UI menampilkan base seperti biasa plus cincin tipis yang
  menunjukkan nilai efektif termodulasi. MIDI, UI, preset, undo semuanya tetap beroperasi pada base.
- Batas awal: parameter float saja, maksimal 4 modifier, evaluasi bebas alokasi di audio thread.
- Contoh pakai yang langsung berguna: wah (Contour freq ← pedal ekspresi), tremolo (MasterOut/level ←
  LFO sync tempo), auto-filter funk (Contour ← envelope follower).

**Effort:** ~1.5–2 weekend (estimasi P4-4 tetap).

### P6-3. Perform view — rencana UI/UX (menyerap P2-5) — INTI SELESAI (v0.19.0)

> **Terkirim.** Saklar **Perform | Edit** di bar atas (pilihan diingat di localStorage). Edit = tampilan lama, tak berubah. Perform (`PerformView.tsx`) = satu layar tanpa scroll: nama preset besar + ‹ ›, BPM besar + tap tempo, **4 tombol scene raksasa** (≥120 px), meter LED In/Out lebar + tombol Tuner/Bypass/Mute besar, dan tuner besar yang menggantikan grid scene saat aktif. Keyboard: **1–4** scene, **←/→** preset, **T** tap (Esc/B tetap global). Diuji: 8 test `PerformView`, 1 E2E. **Yang belum (menyusul):** 8 knob "pin" per preset (butuh simpan pin di metadata preset) dan huruf channel di tombol scene. Rencana desain asli di bawah.



**Masalah hari ini, dideskripsikan jujur:** UI sekarang adalah satu halaman padat — rack efek di kolom
kiri, panel-panel (device, tuner, tempo, scene, preset, MIDI, NAM, IR, performa) bertumpuk di kolom
kanan. Untuk *menyetel* suara ini bagus: semuanya terlihat, semuanya sejangkauan scroll. Untuk *bermain*
ini buruk: tombol scene kecil dan terkubur di tengah sidebar, meter kecil di bar atas, nama preset
tidak menonjol, dan tidak ada satu layar pun yang bisa dibaca dari jarak 2 meter sambil memegang
gitar. FM9 menyebut perbedaan ini "postur edit vs postur perform" — dua pekerjaan yang layarnya memang
tidak boleh sama.

**Rencana konkret — dua view, satu saklar:**
- Bar atas mendapat saklar **Perform | Edit**. **Edit** = seluruh tampilan sekarang, tidak berubah
  sedikit pun. Library dan Settings tidak dijadikan tab terpisah (berbeda dari mockup 4-tab lama):
  keduanya sudah nyaman sebagai panel sidebar di Edit, dan menyebar yang sudah berfungsi hanya
  menambah klik. Multi-view penuh menyusul kalau panelnya benar-benar sesak.
- **Perform** = satu layar tanpa scroll, disusun untuk dibaca jauh dan diklik kasar:
  - **Baris atas:** nama preset dalam huruf besar (elemen paling menonjol di layar) dengan tombol
    ‹ › untuk pindah preset; di kanannya BPM besar + tombol tap tempo selebar telapak.
  - **Tengah, bagian terbesar:** 4 **tombol scene raksasa** (target klik ≥ 120 px — cukup untuk layar
    sentuh atau mouse yang dioperasikan kaki), masing-masing menampilkan nama scene, keadaan aktif
    yang menyala terang, dan (setelah P6-1) deretan huruf channel efek-efek pentingnya.
  - **Bawah:** meter input/output besar bergaya LED tersegmen + indikator gate/comp/limiter; tombol
    Bypass dan Mute besar (fungsi yang sama dengan B/Esc hari ini, kini terlihat).
  - **Kolom kanan:** hingga **8 knob pin** — parameter pilihan user untuk preset itu (disimpan di
    metadata preset, di luar `state` supaya snapshot DSP tidak berubah). Di Edit, tiap knob mendapat
    ikon pin kecil; yang dipin muncul besar di Perform. Ini terjemahan langsung Perform Page FM9:
    dari ~150 parameter, yang disentuh saat manggung hanya segelintir — biarkan user memilihnya.
  - **Tuner:** tombol toggle besar; saat aktif, strip tuner lebar menggantikan baris meter.
  - **Keyboard di Perform:** angka **1–4** = recall scene, **panah kiri/kanan** = ganti preset,
    **T** = tap tempo (Esc mute dan B bypass sudah global hari ini dan tetap). Semua juga bisa dari
    MIDI lewat P6-5, jadi footswitch murah pun menjadikan Perform view layar panggung sungguhan.
- View yang dipilih disimpan di localStorage; aplikasi dibuka kembali pada view terakhir.

**Effort:** ~1–1.5 weekend. **Ketergantungan:** paling bernilai setelah P6-1 (tombol scene menampilkan
channel) dan P5-2 (memoisasi dulu, supaya layar baru tidak menambah beban render yang sudah boros).

### P6-4. Spillover antar preset (verifikasi dulu)

FM9 membiarkan ekor delay/reverb meluruh melewati pergantian preset. MilodikFX sudah punya spillover
antar scene; **antar preset SELESAI diverifikasi (22 Jul 2026)** — dan ternyata sudah benar secara
konstruksi. Load preset lewat `PresetsHandler` memanggil `ParameterRegistry::applyState`, yang **hanya**
memegang closure `get`/`set`/`setEnabled`; ia tidak punya referensi ke `reset()` sama sekali, jadi tidak
mungkin mengosongkan delay line. Preset dengan delay mati mendarat di `setEnabled(false)` dengan spillover
menyala, tempat jalur feedback tetap berjalan dan ekornya terus berdering. Dikunci dengan regression test
di `DspTests.cpp` ("A repeat still ringing survives the effect being switched off"): impuls di-ring saat
aktif, delay dimatikan seperti load preset, blok senyap berikutnya masih membawa echo. Perubahan time
delay saat ekor berbunyi sudah aman sejak dulu (read offset di-smooth, alokasi delay line maksimal sejak
prepare).

### P6-5. MIDI → scene & channel — SELESAI (v0.18.0)

> **Terkirim.** `Mapping` sekarang punya `kind` (parameter/scene/channel) + `index`; `applyControlChange` menangani scene/channel pada tekan (di-post ke message thread lewat `onSceneRecall`/`onChannelSelect`). `SceneManager` juga menyimpan indeks channel per efek dan memanggilnya saat recall (tautan scene→channel yang tertunda dari P6-1). API `learn`/`PUT mappings` menerima `kind`; UI MidiMapping menawarkan target Scene 1–4 dan Channel A/B/C/D. Serialisasi settings `midi.cc.<n>.{kind,effect,parameter,index,mode}`, back-compat file lama. Diuji: 3 test dispatch backend + scene→channel di `SceneTests`, 1 test frontend, 1 E2E.



Mapping MIDI sekarang hanya CC→parameter dan PC→preset (diverifikasi di `MidiController.cpp`). Untuk
panggung, scene (dan setelah P6-1, channel) harus bisa dipanggil dari kaki: perluas target mapping
dengan tipe `scene` (CC value memilih slot, atau satu CC per slot mode toggle) dan `channel`
(efek+indeks). Scene recall hanya menulis enable flags (atomics) jadi aman dari MIDI thread; penulisan
settings tetap lewat message thread seperti sekarang. UI MIDI Learn mendapat dua pilihan target baru.
**Effort:** ~0.5 weekend.

### Urutan pengerjaan yang disarankan

1. **P5-2** memoisasi (jam-jaman, memperbaiki fondasi sebelum layar baru),
2. **P5-3 + P5-4** kebersihan HTTP/update (jam-jaman),
3. **P6-1** channels (fitur terbesar, membuka nilai scene sesungguhnya),
4. **P6-3** Perform view (kini punya sesuatu yang layak ditampilkan besar-besar),
5. **P6-5** MIDI scene/channel (menghidupkan panggungnya),
6. **P6-2** modifier, **P6-4** spillover antar preset, **P5-5** oversampling per voicing — kapan saja,
   saling independen.

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
| 33 | Voicing Centaur + RAT + Big Muff ✅ | P4-1b | ~1–1.5 hari | selesai (v0.14.0) |
| 34 | Update check + sponsor + credit + situs ✅ | P5-1 | ~0.5 weekend | selesai (v0.15.0) |
| 35 | Memoisasi frontend (render 22 Hz) ✅ | P5-2 | ~2–4 jam | selesai (v0.16.0) |
| 36 | Tuner lewat SSE ✅ | P5-3 | ~2–3 jam | selesai (v0.16.0) |
| 37 | UpdateHandler fetch di luar lock + SSE 1-baris ✅ | P5-4 | ~1–2 jam | selesai (v0.16.0) |
| 38 | Oversampling default per voicing ✅ | P5-5 | ~2–3 jam | selesai (v0.16.0) |
| 39 | Channel A/B/C/D per efek ✅ | P6-1 | ~1.5–2 weekend | selesai (v0.17.0); tautan scene→channel di v0.18.0 |
| 40 | Modifier (desain FM9, menggantikan P4-4) | P6-2 | ~1.5–2 weekend | — |
| 41 | Perform view (menyerap P2-5) ✅ (inti) | P6-3 | ~1–1.5 weekend | inti selesai (v0.19.0); pinned knobs menyusul |
| 42 | Spillover antar preset (verifikasi) ✅ | P6-4 | ~2–4 jam | selesai |
| 43 | MIDI → scene & channel ✅ | P6-5 | ~0.5 weekend | selesai (v0.18.0) |

Total estimasi kalau semua dikerjakan: kira-kira 27–35 weekend, dengan catatan NAM adalah yang paling tidak pasti dan bisa melar jauh dari estimasi tergantung hasil tahap riset.

**Urutan dalam P4** (bukan urutan nomornya): P4-0 input gain dulu karena paling kecil dan memperbaiki segala yang di hilirnya; lalu P4-2 spillover karena kecil dan langsung memperbaiki fitur scene yang sudah ada; baru P4-1 tipe overdrive yang jadi pekerjaan besarnya; sisanya menyusul.

**Catatan adaptasi mockup:** dari seluruh elemen mockup `docs/E0B4AFA6-...png`, hanya satu yang sengaja tidak diadaptasi — ADD BLOCK / CLEAR CHAIN (chain dinamis), karena bertabrakan dengan arsitektur satu-prosesor-per-tipe dan urutan chain tetap; alasan lengkap di catatan Revisi 2 di atas. Sisanya tersebar: chain strip & kartu pedal → P1-5, Global Bypass → P1-6, tuner → P0-2, EXP/assign → P0-3, scene → P2-3, tap tempo & sync → P2-4, tab navigasi → P2-5, star/notes preset → P3-1, import/export → P3-7, metronome → P3-8, CPU history & versi → P3-9, meter LED tersegmen → P3-6b. Elemen mockup yang sudah ada di aplikasi sekarang (tidak perlu item): pemilihan device/sample rate/buffer, indikator AUDIO RUNNING, master volume + mute, readout CPU/kHz/samples.
