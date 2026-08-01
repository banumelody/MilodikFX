# P17–P18 — Panel: kenop yang terbaca, dan rentang yang terasa benar

*Direncanakan 1 Agu 2026, dari diskusi apakah sebagian parameter idealnya slider ketimbang kenop.
Lanjutan dari [`material-plan.md`](material-plan.md), yang sudah terkirim sebagai v0.32.0.*

## Keputusan yang dicatat: rack tetap kenop

Bukan karena kepadatan — meski itu benar — melainkan karena **hardware-nya memang begitu**, dan
kesetiaan pada hardware justru tujuan yang diminta.

| Blok | Hardware aslinya |
|---|---|
| Overdrive / fuzz | kenop, tanpa kecuali |
| EQ & Contour | kenop — ini tone stack amp, bukan graphic EQ |
| Compressor | kenop — 1176, LA-2A, dbx 160 |
| Noise gate | kenop — Drawmer, dbx |
| Delay & Reverb | kenop — TC 2290, Eventide, Lexicon |
| **Mixer A/B** | **fader** — ini fungsi console |

Fader di dunia audio praktis hanya muncul di dua tempat: **console** dan **graphic EQ**. EQ di sini
tiga band shelving, yang berarti ia tone stack amp.

Dua alasan pendukung, dicatat supaya tidak dibahas ulang:

- **Gestur kenopnya sudah vertikal.** `Knob` memakai drag vertikal relatif, jadi keuntungan
  ergonomis utama fader sudah ada. Mengubah bentuknya tidak mengubah cara tangan bekerja.
- **Kurva EQ sudah digambar.** `ToneCurve` menggambar respons sungguhan, bukan kurva yang
  disimpulkan dari deretan posisi fader — yang merupakan satu-satunya alasan kuat memakai fader
  untuk EQ.

---

## v0.33.0 — "Panel" (P17)

**Tujuan:** empat hal yang dimiliki kenop hardware dan belum dimiliki kenop di sini.

### P17-1. Tanda skala

Yang paling universal di hardware dan sama sekali belum ada. Tiap pedal punya angka atau titik
tercetak mengelilingi kenopnya.

Ada alasan teknis kenapa ini penting **khusus di aplikasi ini**: kenop hardware itu **absolut** —
posisi penunjuk *adalah* nilainya, selalu. Kenop di sini drag-relatif, yang lebih enak untuk mouse
tapi membuat tampilannya jadi satu-satunya acuan absolut. Tanda skala mengembalikan apa yang panel
sungguhan berikan gratis.

Titik kecil di sepanjang busur 270°, digambar di SVG yang sudah ada.

### P17-2. Tanda tengah untuk parameter bipolar

Bass/mid/treble di jam 12, pan di tengah, asymmetry di nol. Di amp sungguhan tanda ini dicetak
lebih tebal. Diterapkan otomatis ketika `min < 0 < max` — tidak ada daftar yang harus dijaga.

### P17-3. Ukuran berjenjang

Pedal sungguhan punya satu kenop besar dan beberapa yang kecil; hierarkinya terbaca sebelum
labelnya dibaca. Aplikasi ini memakai satu ukuran untuk semuanya, jadi hierarki itu hilang.

Aturannya harus **diturunkan, bukan didaftar**: parameter pertama tiap efek adalah yang utama
(`drivePct`, `thresholdDb`, `timeMs`), jadi ia digambar lebih besar. Sebuah daftar per-efek akan
jadi hal berikutnya yang harus diperbarui setiap blok baru — persis pajak yang v0.32 tolak.

### P17-4. Label tersablon

Label huruf kecil di atas permukaan enclosure yang sekarang akan terbaca sebagai sablon panel,
bukan teks mengambang. Perubahan CSS, bukan struktur.

### P17-5. Mixer A/B jadi sepasang fader vertikal

Satu-satunya blok yang hardware-nya memang console, dan satu-satunya tempat di aplikasi ini di mana
tugasnya adalah **membandingkan dua nilai satu sama lain**. Dua tinggi jauh lebih mudah dibandingkan
daripada dua sudut putar.

Kartunya cocok: parameternya sedikit (levelA, panA, levelB, panB, invertB), jadi tingginya ada.

- **Pan tetap kenop.** Semua console memakai kenop untuk pan, dan alasannya bagus: pan bipolar
  mengelilingi titik tengah, dan rotasi mengungkapkan "kiri/kanan dari tengah" secara alami.
- **Gesturnya harus identik** dengan `Knob` — drag vertikal relatif, shift halus, roda, dobel-klik
  ke default, keyboard penuh. Sebuah fader yang berperilaku berbeda dari kenop di sebelahnya adalah
  bentuk kedua yang harus diingat, dan itu biaya yang harus dibayar hanya sekali di sini.
- **Perform view tidak terpengaruh** — ia tidak menampilkan kartu Mixer.

**Effort: ~1–1.5 weekend.** Semuanya komponen bersama; blok ke-27 tidak menagih apa pun.

---

## v0.34.0 — "Rentang yang terasa benar" (P18)

**Tujuan:** memperbaiki hal yang sebenarnya membuat sebagian kenop terasa salah.

### Ini bukan dugaan — angkanya sudah diperiksa

Rentang yang ada sekarang dipetakan **linear** ke jarak drag. Untuk parameter waktu dan frekuensi,
itu menaruh seluruh wilayah yang berguna di beberapa persen pertama:

| Parameter | Rentang | Rasio | Wilayah berguna di ujung bawah |
|---|---|---|---|
| `compressor.attackMs` | 0.1–200 ms | **2000×** | 0.1–5 ms = **2.5 %** pertama dari drag |
| `noiseGate.attackMs` | 0.1–50 ms | 500× | 0.1–5 ms = 10 % |
| `reverb.releaseMs`-kelas | 5–2000 ms | 400× | |
| `delay.timeMs` | 10–1000 ms | 100× | |
| `reverb.decayTime` | 0.2–10 s | 50× | |
| `dampingHz` | 500–20000 Hz | 40× | |
| `split.freqHz` | 60–2000 Hz | 33× | |
| `compressor.ratio` | 1–20 :1 | 20× | 1–4:1 = 16 % |

Attack compressor adalah kasus terburuk dan juga parameter yang paling menuntut presisi. Satu piksel
drag di ujung bawah melompat beberapa milidetik.

**Dan mengganti kenop jadi fader tidak akan memperbaikinya** — jumlah pikselnya sama. Itu sebabnya
perbaikan ini terpisah dari P17: kalau ini penyakitnya, mengganti bentuk kontrol akan terasa gagal.

### P18-1. Aturannya: waktu dan frekuensi dapat kurva, dB dan persen tidak

Prinsip, bukan daftar:

- **Waktu dan frekuensi** dipersepsi secara logaritmik, jadi drag-nya logaritmik.
- **dB sudah logaritmik** terhadap besaran yang diwakilinya, jadi linear-dalam-dB sudah benar.
- **Persen dan 0..1** adalah rentang persepsi langsung dan tetap linear.

Itu menutup setiap baris di tabel di atas tanpa satu pun pengecualian yang harus dihafal.

### P18-2. Di mana kurvanya hidup — dan di mana tidak

`ParameterDescriptor` mendapat field `skew`, diisi di `ChainFactory.cpp` bersama parameternya. Satu
tempat, sesuai aturan proyek.

**Tapi ia sengaja tidak diterapkan ke `NormalisableRange` milik VST3.**

Alasannya konkret: mengubah skew sebuah `AudioParameterFloat` mengubah pemetaan nilai
ternormalisasi, jadi **setiap lajur otomasi DAW yang sudah ada akan bergeser** — sebuah lajur yang
menulis 0.5 mendarat di tempat lain. Itu harga yang tidak sebanding untuk masalah gestur mouse, dan
host punya kurva otomasinya sendiri.

Jadi: nilai yang tersimpan, API, preset, dan otomasi **tidak berubah sama sekali**. Yang berubah
hanya seberapa jauh kamu harus menarik. Ini juga membuatnya mudah dibatalkan kalau ternyata tidak
disukai.

**Test:** tiap parameter ber-skew tetap mencapai min dan max persis; nilai tengah drag mendarat di
tengah geometrik, bukan aritmetik; parameter tanpa skew tidak berubah satu langkah pun.

**Effort: ~0.5–1 weekend.**

---

## Sengaja tidak dilakukan

| | Alasan |
|---|---|
| **Fader di rack** | Hardware-nya kenop, gesturnya sudah vertikal, dan kurva EQ sudah digambar terpisah. Tiga alasan, dan tidak satu pun soal kepadatan. |
| **Skew pada `NormalisableRange` VST3** | Menggeser setiap lajur otomasi yang sudah ada, demi masalah yang hanya menyangkut drag mouse. |
| **Slider horizontal untuk blend dua arah** | Sempat dipertimbangkan untuk `cabinet.irBlend` dan `mix`. Memakan lebar — sumbu yang paling sempit di kartu tiga kolom — dan manfaatnya belum jelas. Dilihat dulu setelah P17. |
| **Angka tercetak di sekeliling kenop** | Titik cukup. Angka pada kenop 76 px akan terlalu kecil untuk dibaca dan menambah derau di kartu yang sudah padat. |

## Total

**±1.5–2.5 weekend**, dua titik rilis. v0.33 adalah yang diminta; **v0.34 adalah yang saya duga
sebenarnya terasa kurang**, dan angkanya di atas membuat dugaan itu bisa diperiksa sebelum
dikerjakan.
