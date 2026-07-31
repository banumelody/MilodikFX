# P11–P13 — Panel input, board kosong, dan blok ganda

*Direncanakan 31 Jul 2026, dari diskusi panjang yang memvalidasi dua sumber utama: Logic Pro
Pedalboard (Router, Splitter/Mixer otomatis) dan Fractal FM9 (blok Input, inventaris blok tetap,
rig magnetik+piezo). Prototipe interaktifnya bisa dicoba di
<https://claude.ai/code/artifact/62bb34ac-fd11-423c-9ef6-16ef0e4c7603>; alur UX-nya di
[`pedalboard-ux.md`](pedalboard-ux.md).*

Tiga rilis, masing-masing berdiri sendiri dan bisa berhenti kapan pun tanpa ada yang setengah
jadi — pola yang sama dengan v0.25→v0.28:

```mermaid
flowchart LR
    A["v0.29\nPanel input\nport mapping + split L/R"] --> B["v0.30\nBoard kosong\nSplitter/Mixer sebagai blok"]
    B --> C["v0.31\nInventaris blok ganda\n3 overdrive, 2 delay, ..."]
    A -.->|"rig magnetik+piezo\nselesai di sini"| A
    C -.->|"bongkar batas 16 stage\nrisiko tertinggi, paling akhir"| C
```

Urutannya disengaja: yang paling kecil dan paling langsung berguna (rig dua-pickup) duluan; yang
menyentuh jantung audio thread (repack urutan) paling akhir, saat dua rilis sebelumnya sudah
membuktikan bentuk UX-nya benar.

---

## v0.29.0 — "Panel input" (P11)

**Tujuan:** gitar dengan dua output jack — magnetik + piezo — masuk ke Scarlett port 1 dan 2,
menjadi dua chain penuh dengan kenop masing-masing, digabung di Mixer. Ditambah: memilih port
fisik untuk tiap kanal, karena hari ini engine hanya pernah melihat dua channel pertama yang
kebetulan diserahkan JUCE.

### Model tiga lapis

Adaptasi blok Input FM9, dipotong di tempat yang benar. FM9 boleh menulis "Input 2" di preset
karena semua FM9 punya panel belakang yang sama; kita jalan di interface sembarang, jadi ada satu
lapisan pemetaan:

```mermaid
flowchart LR
    subgraph fisik["Port fisik (pengaturan perangkat)"]
        P1["Input 1 · INST"]
        P2["Input 2 · INST"]
        P3["Input 3 · LINE"]
        P4["Input 4 · LINE"]
    end
    subgraph logis["Kanal logis (yang disebut preset)"]
        L["kanal L"]
        R["kanal R"]
    end
    subgraph board["Board (preset)"]
        JA["jalur A"]
        JB["jalur B"]
    end
    P1 -->|audio.inputPortL| L
    P2 -->|audio.inputPortR| R
    L -->|"split.mode = L/R"| JA
    R -->|"split.mode = L/R"| JB
```

**Preset menyebut L/R, perangkat memetakan port.** Preset "Ambient Lead" tetap bermakna di
interface lain; "gitar di port 3" tidak. Mode blok Input FM9 (Left Only / Right Only / Sum L+R /
Stereo) sudah kita punya persis sebagai `input.mode` — tidak berubah.

### P11-1. Engine: buka semua port + pemetaan

`AudioDeviceController` hari ini membuka `initialise(2, 2, ...)` — di 4i4, port 3/4 secara
harfiah tidak bisa dijangkau. Diubah: buka semua channel input device (praktisnya ≤8), lalu
petakan dua port pilihan ke kanal L/R engine di titik `MainComponent` menyalin
`inputChannelData`.

**Jebakan yang menentukan implementasi:** JUCE menyerahkan `inputChannelData` **berdasarkan
urutan channel aktif, bukan nomor port fisik**. Kalau yang aktif port 2 dan 4, `in[0]` = port 2.
Lapisan pemetaan harus menerjemahkan *port fisik → posisi di antara channel aktif*, dihitung
ulang **setiap device berubah** — bukan sekali di startup. Salah di sini gejalanya bukan crash,
tapi sunyi atau sinyal dari port yang salah.

- Persist: `audio.inputPortL` / `audio.inputPortR` di settings file (bukan preset), disimpan
  sebagai **nama channel** (dari `getInputChannelNames()`; ASIO memberi nama sungguhan), resolve
  ke indeks saat device dibuka. Nama tak ditemukan → fallback dua channel pertama, dicatat di log.
- Meter input tidak berubah: ia mengetuk *setelah* pemetaan, aritmetika `inputDb + trimDb` tetap
  benar.

### P11-2. DSP: mode Splitter `L/R` + polaritas Mixer

`SplitProcessor::Mode` bertambah satu nilai: `leftRight`. Semantiknya semantik "Left Only" FM9,
diduplikasi mono ke tiap jalur:

```
divide():  pathA[0] = pathA[1] = in[0]      // kanal L, mono ke jalur A
           pathB[0] = pathB[1] = in[1]      // kanal R, mono ke jalur B
```

Mono-duplikat, bukan satu sisi kosong — supaya stage dual-mono di hilir memproses sinyal di
tengah, dan pan di Mixer yang menempatkannya di bidang stereo.

```mermaid
flowchart LR
    MAG["magnetik → port 1"] --> L["kanal L"]
    PIE["piezo → port 2"] --> R["kanal R"]
    L --> S{{"Split · L/R"}}
    R --> S
    S -->|A| OD["Overdrive → NAM → Cabinet"]
    S -->|B| CL["Compressor → EQ → Reverb"]
    OD --> M{{"Mixer"}}
    CL --> M
    M --> OUT["master"]
```

- Mode L/R hanya bermakna saat `input.mode = stereo`. **Tidak dikopel di engine** (parameternya
  ortogonal dan keduanya sah sendiri-sendiri); UI menampilkan chip peringatan saat `split.mode =
  L/R` tapi input mode bukan stereo.
- **`mixer.invertB`** (bool): flip polaritas jalur B. Piezo + magnetik bisa saling membatalkan
  sebagian saat diblend — bukan soal delay (keduanya lewat konverter yang sama, tiba di sampel
  yang sama), murni polaritas transducer. Satu parameter, langsung melayani rig utamanya.
- Test: sinyal hanya-kiri keluar utuh di A dan nol di B (dan sebaliknya, simetris); L/R dengan
  konten identik L=R ≡ mode `even` bit-exact; `invertB` dua kali ≡ tanpa invert.

### P11-3. API + UI

- `/api/devices` membawa daftar port input (nama asli) + pemetaan aktif; `PUT` untuk mengubah.
  Panel Device dapat dua dropdown: *Kanal L ← port*, *Kanal R ← port*.
- `split.mode` bertambah opsi ketiga di kartu Split; router ChainStrip memberi label sumber per
  jalur saat mode L/R (`A ← Input 1 · INST`) — di situ router berhenti jadi hiasan dan mulai jadi
  diagram kabel.
- **Plugin:** panel port disembunyikan (`isPluginHost()` — host yang punya I/O). Mode L/R tetap
  bekerja atas stereo yang host serahkan; didokumentasikan di kartu Split.

### Definition of done v0.29

Suite backend penuh (SplitTests bertambah), vitest + type-check + lint, E2E round-trip pemetaan
port, smoke, pluginval strictness 10, CI hijau, README + landing page + CLAUDE.md (bagian *Audio
device* dan *Parallel paths*), rilis + installer. **Effort: ~1 weekend.**

---

## v0.30.0 — "Board kosong" (P12)

**Tujuan:** board mulai kosong = kabel lurus (shunt-nya Fractal); user menyusun rig dengan drag
& drop dari palet; Splitter dan Mixer adalah blok yang ditaruh, bukan mode yang dinyalakan.
Perilakunya sudah teruji di prototipe (26 pemeriksaan headless); rilis ini memindahkannya ke
aplikasi.

### Model penempatan

Prosesor **tetap semua dibangun** saat startup (registry tidak berubah, daftar parameter VST3
tetap). "Ditaruh di board" adalah status, bukan keberadaan:

```mermaid
stateDiagram-v2
    direction LR
    Unplaced: Tidak di board\n(disabled + tersembunyi)
    Placed: Di board\n(enabled, kartu tampil)
    PlacedOff: Di board, bypass\n(disabled, kartu redup)
    Unplaced --> Placed: seret dari palet
    Placed --> PlacedOff: toggle efek / scene
    PlacedOff --> Placed: toggle efek / scene
    Placed --> Unplaced: tombol × di kartu
    PlacedOff --> Unplaced: tombol × di kartu
```

- **Invarian engine:** tidak di board ⇒ disabled, ditegakkan di satu tempat (handler), bukan
  dipercayakan ke UI.
- **Aturan scene tidak berubah:** scene menyimpan enable flags dan hanya berlaku pada stage yang
  *di board*; penempatan itu milik **preset**, bukan scene — persis FM9 (layout grid per preset,
  scene mem-bypass). Scene yang menata ulang board mid-lagu adalah kejutan yang aturan scene
  larang.
- **Migrasi:** preset schema ≤7 dan settings lama → **semua stage di board** (bunyi tidak berubah
  sesudah update — tidak bisa ditawar). "Board kosong" adalah default aksi *Board baru*, bukan
  default migrasi.

### P12-1. Engine + API

`placed` sebagai himpunan id di samping order (satu snapshot dengan order dan bus — lihat P13-1;
di v0.30 masih muat di mekanisme sekarang). `/api/chain/order` (GET) membawa `placed`;
`PUT /api/chain/board {placed: [ids]}` mengubahnya. Schema preset **8**: `board` tersimpan di
preset dan settings. `applyIds` tetap pemaaf dua arah — id tak dikenal diabaikan, stage yang tak
disebut kembali ke posisi build.

### P12-2. UI: palet + lajur + router

Dari prototipe, dipindah ke React dengan disiplin yang sudah ada (`useChainReorder`, pointer
events, bukan HTML5 DnD):

- **Palet** di sisi kanan, blok dikelompokkan (Dinamika / Drive / Nada / Amp / Ruang / Utilitas);
  seret ke lajur = taruh. Enter/Space menaruh ke ujung jalur A (jalan tanpa pointer).
- **Splitter dari palet membuka jalur B; Mixer menyusul sendiri** di ujung kanan dan ikut hilang
  saat Splitter dibuang (kutipan Apple: *"a Mixer automatically appears at the far right"*).
  Jalan pintas kedua: menjatuhkan blok langsung ke lajur B memasangkan Splitter+Mixer otomatis.
- **Dua aturan letak Logic ditegakkan dengan koreksi, bukan penolakan:** Mixer tidak boleh
  mendahului Splitter-nya (dilipat ke ujung), Splitter tidak boleh jadi blok terakhir (ditarik
  mundur satu). Seretan yang diam-diam mental balik terbaca seperti seretan yang rusak.
- **ChainStrip jadi router dua garis** di antara Splitter dan Mixer — garis **atas B, bawah A**
  (konvensi Apple: A lurus terus, B memutar di atasnya); satu garis di luar seksi paralel.
- Kartu Splitter/Mixer bergaya utilitas (garis putus, warna bus, tanpa nomor instance).

### Definition of done v0.30

26 pemeriksaan prototipe menjadi vitest sungguhan; test migrasi (preset schema 7 → semua
di board); invarian tidak-di-board⇒disabled di suite backend; E2E taruh/buang/round-trip; smoke;
pluginval; CI; dokumentasi + screenshot + landing page. **Effort: ~2 weekend.**

---

## v0.31.0 — "Inventaris blok ganda" (P13)

**Tujuan:** dua overdrive seri di satu jalur; overdrive di A *dan* B dengan kenop terpisah —
cara FM9 (2 Amp, 3 Drive, 4 Delay), bukan registry dinamis. Semua instance dibangun saat
startup dan menunggu di palet; registry tetap immutable, daftar parameter VST3 tetap tetap.

### Inventaris

| Tipe | Jumlah | Alasan batas |
|---|---|---|
| Overdrive | **3** | tumpuk seri (screamer → fuzz) + satu di jalur lain |
| NoiseGate, CleanBoost, Compressor, EQ, Contour, Cabinet, Delay, Reverb | **2** | satu per jalur |
| **NAM** | **1** | satu model Standard ≈ 29% budget CPU di 96 kHz/32 sampel — bukan batas format |
| Input, Split, Mixer, Master | 1 | pinned / utilitas |

Total **24 stage** (sekarang 14), ±133 parameter (sekarang 70). Angka jatah adalah tuas, bukan
arsitektur — tapi lihat *Prasyarat pengukuran* di bawah sebelum menguncinya.

### Skema id: instance 1 tidak berganti nama

**`overdrive` tetap `overdrive`; yang baru adalah `overdrive2`, `overdrive3`.** Ini keputusan
paling penting di P13, karena menghapus hampir seluruh masalah migrasi:

- Preset, settings, MIDI mapping, modifier, pin, dan channel lama semua merujuk id polos — dan id
  itu **masih ada**. Tidak ada tabel migrasi.
- Parameter VST3 bertambah **append-only** — lajur otomasi yang sudah ada di proyek DAW lama
  tidak bergeser. (`Overdrive 2 Drive` terbaca di DAW, bukan `Slot 7 Param 3` — inilah alasan
  memilih inventaris tetap ketimbang slot generik.)
- Aturan baca satu baris: id tanpa angka = instance 1. Label UI menomori hanya bila tipe itu
  bisa ganda, jadi board yang memakai satu-satu terbaca persis seperti sekarang.

### P13-1. Engine: snapshot urutan menggantikan packing 4-bit

Urutan hari ini dikemas 4 bit per stage ke satu `atomic<uint64_t>` — muat 16, dan 24×4 = 96 bit
tidak muat. Diganti **snapshot yang dipublikasikan lewat pointer** — pola yang sama dengan Slot
NAM, yang sudah terbukti di codebase ini:

```mermaid
sequenceDiagram
    participant C as Thread kontrol (REST)
    participant A as Audio thread
    participant T as Timer MainComponent
    C->>C: bangun OrderSnapshot baru<br/>(urutan + bus + penempatan, satu objek)
    C->>A: staged.exchange(snapshot) — release
    A->>A: awal blok: adopsi staged,<br/>pindahkan yang lama ke retired
    Note over A: satu acquire-load per blok,<br/>tanpa alokasi, tanpa lock
    T->>T: reap retired (bukan di callback)
```

Bonus yang bukan basa-basi: urutan, bus, **dan penempatan** hidup di **satu objek** — hari ini
urutan dan bus adalah dua atomic terpisah, jadi secara teori satu blok bisa melihat urutan baru
dengan bus lama. Snapshot menutup celah itu sekalian.

### P13-2. Instance + pengawatan

- `ChainFactory` membangun dari tabel inventaris; `ChainOrder` memuat id ber-instance.
- `findProcessor<T>()` = "instance pertama" — semua pemakaian di-audit; yang harus menyentuh
  *semua* instance diganti eksplisit. Yang sudah ketahuan: **`global.bpm` harus mendorong tempo
  ke kedua delay**, bukan cuma yang pertama.
- `DRIVE_CONTROLS` di EffectRack di-key lewat tipe (strip angka), bukan id mentah.
- Palet menampilkan **sisa jatah** (`2/3`), habis = redup — ditolak sebelum gestur dimulai.

### Prasyarat pengukuran (sebelum jatah dikunci)

1. **Memori:** tiap instance itu objek DSP sungguhan — dua Cabinet = empat mesin konvolusi, dua
   Delay = dua delay line penuh. Ukur RSS sebelum/sesudah, catat angkanya di dokumen ini.
2. **CPU:** PerformanceTests dapat kasus terburuk baru (semua instance di board, voicing
   termahal); assertion rasio yang build-independent, seperti biasa.

### Definition of done v0.31

Test independensi instance (kenop od1 tidak menyentuh od2), dua-seri ≠ satu (terukur, bukan
diasumsikan), stress reorder saat memproses (snapshot handoff), preset masa depan pemaaf
(`overdrive3` di build ber-jatah-2 diabaikan tanpa error), pluginval dengan ±133 parameter,
E2E, smoke, CI, dokumentasi penuh + angka pengukuran memori/CPU tercatat. **Effort: ~2.5–3
weekend.**

---

## Eksplisit di luar cakupan tiga rilis ini

| Fitur | Kenapa ditunda | Prasyarat kalau nanti |
|---|---|---|
| **Output block ala FM9** (wet/dry ke port 3/4) | `MasterOut` adalah satu-satunya titik akhir dan membawa limiter pengaman — multi-output = beberapa titik akhir = topologi baru | Desain multi-endpoint + limiter per endpoint |
| **Seksi paralel ganda / bersarang** | Engine mengenal satu Splitter + satu Mixer by pointer, satu buffer `pathB` | Daftar bracket + N buffer + aturan sarang |
| **NAM ke-2** | ~58% budget CPU untuk dua model Standard | Terbukti kurang setelah v0.31 dipakai; naikkan jatah = tambah parameter VST3 (append-only, aman) |
| **Multi-entry sungguhan** (4 blok Input FM9) | Satu buffer masuk; mode L/R menutup kasus dua sumber — persis kemampuan fisik 4i4 (2 port INST) | Interface >2 sumber simultan yang nyata dipakai |

## Total

**±5.5–6 weekend**, tiga titik rilis. Risiko terbesar (P13-1, jantung audio thread) ditaruh
terakhir dengan pola handoff yang sudah terbukti di NAM. Tiap rilis berhenti bersih: v0.29 saja
sudah menyelesaikan rig magnetik+piezo; v0.30 saja sudah memberi board kosong; v0.31 melengkapi
cita-cita FM9-nya.
