# Rencana Implementasi NAM — Head Amp Simulator (P2-1)

**Status:** GO — diputuskan 22 Jul 2026 berdasarkan pengukuran di mesin pengembang, bukan estimasi.
**Keputusan produk yang dijawab dokumen ini:** "head/amp simulator bisa diimplementasi tidak?" → **Bisa.** Cabinet simulator sudah ada (analitik + dual IR); bagian *head*-nya adalah NAM, dan angka di bawah membuktikan ia muat di anggaran realtime rig ini.

---

## 1. Dua pertanyaan, satu jawaban

**"Head cabinet simulator bisa diimplementasi?"** Cabinet-nya **sudah selesai** — analitik + IR loader + dual-IR blend (P4-3). Yang belum ada adalah *head*: voicing preamp + kompresi power amp. Keputusan lama (tercatat di roadmap P4) tetap berlaku: **jangan** bangun head sim analitik buatan tangan — tipe drive amp-in-a-box menutup kebutuhan ringannya, dan NAM menutup kebutuhan sebenarnya dengan kualitas yang tidak terkejar model buatan tangan.

**"NAM bisa diimplementasi?"** Bisa. Semua blocker yang mungkin sudah diperiksa satu per satu:

| Aspek | Hasil |
|---|---|
| Lisensi | MIT (core) + MPL2 (Eigen) + MIT (nlohmann) — semua kompatibel AGPLv3. Model = data, bukan kode ter-link. |
| API | `get_dsp(path)` → `unique_ptr<DSP>`, `Reset(rate, maxBuffer)`, `process(float**, float**, n)` dengan `NAM_SAMPLE_FLOAT`. |
| RT-safety | `process()` terverifikasi bebas alokasi (upstream punya test `allocation_tracking.cpp`; fix LSTM #299 sudah masuk). |
| MSVC | **Terbukti langsung**: spike terkompilasi bersih di MSVC /MT /O2 /arch:AVX2 percobaan pertama, tanpa patch. Bahaya alignment Eigen (#67) tidak muncul. |
| CPU | **Terukur, lihat §2.** Ini dulunya satu-satunya unknown penentu. |

## 2. Hasil spike — angka yang menentukan

Harness: `spike/nam-bench/` (tercommit, bisa direproduksi). NAM core di-pin ke commit **`3cde95c`** (8 Jul 2026, setelah fix RT-safety #299), clone di `build-spike/nam` (di luar version control). Build Release /MT /arch:AVX2 /fp:fast, `NAM_SAMPLE_FLOAT`, prioritas proses High.

Anggaran: satu blok host 32 sampel @ 96 kHz = **333 µs**. Model berjalan di 48 kHz, jadi satu blok host = **16 frame** untuk model — kolom pertama adalah kasus nyata.

| Model | 16 frame (mean) | p99 | % anggaran (mean / p99) | Load | Reset+prewarm |
|---|---|---|---|---|---|
| **WaveNet A1-Standard** | 29,3 µs | 47,7 µs | **8,8% / 14,3%** | 16 ms | 6 ms |
| **WaveNet A2 (full)** | 35,4 µs | 56,4 µs | **10,6% / 16,9%** | 21 ms | 7 ms |
| **LSTM** | 1,8 µs | 2,8 µs | **0,5% / 0,8%** | 9 ms | 3 ms |
| WaveNet mini (test) | 3,0 µs | 5,0 µs | 0,9% / 1,5% | 9 ms | 0 ms |

Kesimpulan yang dibawa angka ini:

- **Ketakutan "32 sampel patologis untuk NAM" tidak terbukti di desktop x86 + AVX2.** Laporan komunitas soal "128 sweet spot, 64 sudah krek-krek" berasal dari ARM embedded dan plugin lengkap (EQ+gate+normalisasi). Model telanjang di mesin ini: 8,8%.
- Overhead per-blok memang ada (8,8% @16 frame vs 6,1% @128) tapi landai, bukan tebing.
- **Proyeksi total**: chain sekarang 7,6% (semua nyala) + A1-Standard ~9–11% + resampler (≈0) ≈ **17–19% rata-rata**. Kasus terburuk gabungan (MIAB 8x + NAM) ≈ 27% — masih hampir 4x headroom.
- Ganti model = ~25 ms kerja off-thread. Tak terasa.
- `wavenet_a1_standard.nam` tidak punya field `sample_rate` (−1) — fallback "anggap 48 kHz" **benar-benar terpakai** di dunia nyata, bukan teoretis.

**Peringatan yang tetap berlaku:** angka Debug akan 10–100x lebih lambat (sifat Eigen tanpa optimasi) — unit test performa NAM harus pakai model LSTM/mini atau rasio, jangan gerbang absolut di Debug.

## 3. Desain teknis

### 3.1 Posisi di chain

```
InputTrim → NoiseGate → CleanBoost → Compressor → Overdrive → EQ → Contour → [NAM] → Cabinet → Delay → Reverb → MasterOut
```

Pedal masuk ke head, head masuk ke cab — urutan rig sungguhan. `NamProcessor` adalah tipe baru (aturan satu-prosesor-per-tipe `findProcessor<T>` aman), **default mati**, toggleable. Jumlah stage 11 → 12; test `ChainFactory` ikut berubah.

### 3.2 `NamProcessor` — kontrak realtime

- **Muat model** (`get_dsp` + `Reset` + prewarm) di thread REST/message, **tidak pernah** di audio thread — pola yang sama dengan IR loader.
- **Serah-terima tiga-atomic, tanpa lock, tanpa free di audio thread** (plugin resmi *melanggar* ini — men-destruct model lama di dalam callback; kita tidak meniru):
  - `staged` (`atomic<DSP*>`): loader menaruh model baru yang sudah di-Reset.
  - Audio thread, di awal blok: jika `staged` ≠ null dan `retired` kosong → tukar active↔staged, taruh yang lama di `retired`.
  - `retired` (`atomic<DSP*>`): timer message-thread (sudah ada di `MainComponent`) mengambil dan men-delete. Reaper, bukan callback.
- **`Reset` di `prepareToPlay`** aman: device restart menghentikan callback dulu (disiplin yang sama dengan `IrEngine.prepare`). Model aktif di-Reset ulang di sana dengan rate/buffer baru.
- **Resampler 96↔48**: vendor `ResamplingContainer` dari AudioDSPTools (MIT, Lanczos A=12 — bar kualitas yang dipakai plugin resmi; sudah ada di submodule clone). Menangani rasio arbitrer, jadi model 44,1 kHz pun jalan. **Wajib diaudit dulu**: alokasi hanya di reset-path, tidak per-blok (unknown riset yang belum tertutup — lihat todo #2). Fallback kalau audit gagal: `juce::WindowedSincInterpolator`.
- Rantai proses: `inputDb` (gain masuk, smoothed) → resample turun → `model->process()` → resample naik → `outputDb` (makeup, smoothed). Tanpa model termuat: passthrough murni.
- Latensi resampler+model dibaca sekali saat load, diekspos informasional di API.
- `GetExpectedSampleRate()` dihormati; −1 → 48 kHz (konvensi ekosistem, terverifikasi terhadap sumber).

### 3.3 AVX2 dan build portable

`/arch:AVX2` di-scope **hanya ke static lib NAM**, bukan seluruh exe. Build portable yang dibagikan ke teman bisa jatuh di CPU pra-2013: tambahkan cek runtime (`juce::SystemStats` extension flags) — tanpa AVX2, pemuatan model **ditolak dengan pesan jelas**, bukan crash illegal-instruction. Vendoring via FetchContent, pin `3cde95c`, define `NAM_SAMPLE_FLOAT` + `EIGEN_MPL2_ONLY` + `NOMINMAX` + `WIN32_LEAN_AND_MEAN` konsisten di semua TU (bahaya ODR).

### 3.4 Library, API, registry, UI

Semua meniru pola IR yang sudah terbukti:

- **`NamLibrary`** (`src/preset/`): `Documents\MilodikFX\NamModels`, sanitasi nama identik `IrLibrary`.
- **`NamHandler`** (`/api/nam`): GET daftar + metadata (author, gear, tone type — gratis dari file), POST import (base64, cap ukuran — file .nam ≤ ~3 MB), POST reveal.
- **Registry** (`ChainFactory.cpp`, satu-satunya tempat): efek `nam` — `enabled`, `namFile` (parameter teks + options), `inputDb` (−12…+12), `outputDb` (−12…+12). Otomatis muncul di REST, UI, preset, settings.
- **UI**: kartu rack ter-generate otomatis (select parameter teks sudah ada); panel folder seperti IR. Metadata model tampil di kartu (fase 2).
- Model **tidak** di-bundle — model test upstream berbobot acak (bukan capture nyata); pengguna ambil dari TONE3000. Unit test memakai `example_models/` dari checkout FetchContent, tidak ada .nam masuk repo.

## 4. Todo — urut pengerjaan

| # | Item | Isi | Test | Estimasi |
|---|---|---|---|---|
| 1 | **Vendoring + build** | FetchContent pin `3cde95c`; target static `milodikfx_nam` (glob `NAM/*.cpp` + `NAM/*/*.cpp`), AVX2 scoped, defines konsisten; link ke app + tests + plugin | build hijau 3 target | 0,5 hari |
| 2 | **Audit + vendor resampler** | Baca `ResamplingContainer::Reset()`/proses; pastikan alokasi hanya di reset-path; ukur latensi nyata via `GetLatency()` | unit: no-alloc per blok, latensi deterministik, roundtrip 96→48→96 null-ish untuk sinyal band-limited | 0,5 hari |
| 3 | **`NamProcessor`** | Stage/active/retired handoff; reaper di timer `MainComponent`; rantai gain→resample→model→resample→gain; passthrough tanpa model; re-Reset di prepareToPlay; cek AVX2 runtime | unit: passthrough bit-identik tanpa model; muat `wavenet_a1_standard.nam` → keluaran finite & berubah; **stress handoff** (loop load di thread lain sambil processBlock jalan — pola `ConcurrentPrepareTests`); device-restart; enable/disable | 1 hari |
| 4 | **Library + API + registry** | `NamLibrary`, `NamHandler`, registrasi efek `nam` di `ChainFactory` | unit: sanitasi path (pola `IrLibrary`), import/list/resolve; registry: param muncul di kedua build; preset round-trip `namFile` | 0,5–1 hari |
| 5 | **UI + E2E** | Kartu otomatis; panel folder; string Indonesia | vitest kartu; E2E: pilih model → engine konfirmasi, tolak file non-.nam, param bertahan reload | 0,5 hari |
| 6 | **Perf + verifikasi live** | Kasus NAM di `PerformanceTests` (model kecil utk Debug, rasio bukan absolut); ukur %DSP Release di rig dengan model Standard sungguhan dari TONE3000 | target: total < 25% dgn semua nyala + NAM Standard | 0,5 hari |
| 7 | **Rilis 0.12.0** | Bump, 3 artefak, verifikasi unduh + jalan | smoke penuh | 0,25 hari |

**Total: ±3,5–4 hari kerja** — konsisten dengan estimasi lama "3–5 weekend" tapi kini tanpa unknown penentu.

**Fase 2 (setelah dipakai, opsional):** kalibrasi level dBu (`input_level_dbu` → gandeng InputTrim; kebanyakan model komunitas mengosongkannya, jadi bukan blocker v1), dukungan `.namb` biner, tampilan metadata lengkap, scene/MIDI binding untuk `namFile`.

## 5. Risiko tersisa — jujur

1. **Model test upstream ≠ capture nyata.** Angka spike dari arsitektur yang benar, tapi verifikasi akhir (todo #6) harus memakai model TONE3000 sungguhan di rig, dengan telinga.
2. **`main` tanpa tag stabil.** Pin `3cde95c`; bump = tanggung jawab kita, MSVC tidak dites upstream. Mitigasi: test suite kita sendiri adalah gerbang regresinya.
3. **Audit resampler bisa gagal** (alokasi per-blok). Mitigasi sudah di todo #2: fallback JUCE interpolator.
4. **p99 16,9% (A2) adalah tail satu proses uji**, bukan di bawah beban engine penuh. Kalau xrun muncul di verifikasi live, opsi mundur yang sudah terukur: LSTM/tier ringan (0,5%) tetap luar biasa murah.

## 6. Reproduksi spike

```powershell
git clone --depth 1 --recurse-submodules --shallow-submodules `
  https://github.com/sdatkinson/NeuralAmpModelerCore build-spike\nam
cmake -S spike\nam-bench -B build-spike\bench -G "Visual Studio 17 2022" -A x64 `
  -DNAM_DIR="$PWD\build-spike\nam"
cmake --build build-spike\bench --config Release --parallel
build-spike\bench\Release\nambench.exe build-spike\nam\example_models\wavenet_a1_standard.nam
```
