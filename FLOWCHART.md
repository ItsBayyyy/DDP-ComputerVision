# Flowchart Sistem Astral AI

Dokumen ini menjelaskan alur kerja (flowchart) sistem **Astral AI** secara terstruktur, dari tahap input citra hingga interaksi berbasis bahasa natural (NLU). Flowchart merepresentasikan pipeline lengkap Computer Vision → Knowledge Representation → Natural Language Interaction.

---

## 1. Inisialisasi Program

**Start Program**
Program dimulai dari fungsi `main()`.

**Inisialisasi Sistem & Konfigurasi**

* Memuat parameter sistem (`min_area`, `confidence_threshold`, `with_result`).
* Inisialisasi seed random.
* Mengaktifkan dukungan ANSI console (warna, animasi, UTF-8).

Tujuan tahap ini adalah memastikan lingkungan eksekusi siap sebelum pemrosesan citra dilakukan.

---

## 2. Input & Persiapan Gambar

**Input Path Gambar**
Program menerima path file gambar melalui argumen command line.

**Keputusan: Format File?**

* Jika format **bukan PPM** → gambar dikonversi ke PPM menggunakan ImageMagick.
* Jika format **PPM** → langsung diproses.

Tahap ini bertujuan menyeragamkan format citra agar pemrosesan piksel dapat dilakukan secara langsung.

---

## 3. Load Gambar ke Memori

**Baca File PPM**

* Membaca header PPM.
* Mengambil lebar, tinggi, dan nilai maksimum warna.
* Memuat data RGB ke memori (`img_data`).

**Inisialisasi Struktur Data**

* `mask[]` → penanda piksel warna tertentu.
* `labels[]` → penanda hasil segmentasi objek.

---

## 4. Loop Warna (Computer Vision Core)

Proses berikut dijalankan berulang untuk setiap warna: **Merah, Hijau, Biru, Kuning**.

### 4.1 Konversi Warna

**RGB → HSV**
Setiap piksel dikonversi ke ruang warna HSV untuk memudahkan segmentasi berdasarkan warna.

---

### 4.2 Deteksi Warna

**Pengecekan Rentang HSV**

* Jika nilai HSV berada dalam rentang warna target, piksel ditandai pada `mask`.
* Luas area warna dihitung sebagai persentase dari total piksel.

---

### 4.3 Operasi Morfologi

**Erosi → Dilasi**

* Erosi menghilangkan noise kecil.
* Dilasi (iteratif) menyatukan area objek yang terfragmentasi.

Tujuannya menghasilkan mask yang lebih bersih dan stabil.

---

### 4.4 Segmentasi Objek

**Flood Fill Iteratif**

* Menemukan connected component pada mask.
* Menghitung statistik objek: luas area, bounding box, dan titik pusat.

**Keputusan: Luas ≥ `min_area`?**

* Tidak → objek diabaikan (noise).
* Ya → objek disimpan ke basis pengetahuan (`kb_objects`).

---

## 5. Post-Processing Objek

### 5.1 K-Means Clustering

**Klasifikasi Ukuran Objek**
Objek dikelompokkan menjadi:

* Kecil
* Sedang
* Besar

berdasarkan luas area menggunakan K-Means sederhana.

---

### 5.2 Klasifikasi Scene

**Analisis Scene Global**
Berdasarkan:

* Distribusi warna.
* Jumlah objek.

Hasil klasifikasi:

* Pemandangan Alam
* Dokumen/Teks
* Ilustrasi
* Campuran

Disertai alasan klasifikasi yang bersifat heuristik.

---

## 6. Visualisasi Output (Opsional)

**Keputusan: `with_result` Aktif?**

* Ya → gambar output dibuat dengan bounding box dan disimpan.
* Tidak → tahap ini dilewati.

---

## 7. Visualisasi ASCII

**ASCII Scan Animation**

* Menampilkan hasil pemindaian gambar dalam bentuk ASCII grayscale.
* Menandai posisi objek terdeteksi.

Tahap ini bersifat representatif dan informatif untuk antarmuka CLI.

---

## 8. Sistem Siap (Idle State)

**Status Sistem**

* Menampilkan hasil klasifikasi scene.
* Menampilkan jumlah objek terdeteksi.

Program kemudian masuk ke mode interaktif.

---

## 9. Loop Interaksi NLU

### 9.1 Input Pengguna

Pengguna memasukkan pertanyaan dalam bahasa natural.

---

### 9.2 Analisis Intent

**NLU Engine**

* Tokenisasi input.
* Fuzzy matching (Levenshtein Distance).
* Skoring berbasis Bayesian sederhana.

Intent yang didukung:

* Greeting
* Identity
* Count
* Where
* Stats
* Describe
* Scene

---

### 9.3 Keputusan Confidence

**Confidence ≥ Threshold?**

* Tidak → sistem meminta klarifikasi atau memberi sugesti.
* Ya → intent dieksekusi.

---

## 10. Eksekusi Intent

Berdasarkan intent yang terdeteksi:

* **Count** → menghitung jumlah objek.
* **Where** → menampilkan koordinat objek.
* **Stats** → menampilkan statistik warna & objek.
* **Describe** → deskripsi global gambar.
* **Scene** → hasil klasifikasi scene.
* **Greeting / Identity** → respons sosial.

Semua respons berbasis data hasil pemrosesan citra, bukan nilai statis.

---

## 11. Terminasi Program

**Keputusan: Keluar?**

* Jika pengguna mengetik `exit` / `keluar`:

  * File sementara dihapus.
  * Memori dibebaskan.
  * Program selesai.

---

## Ringkasan

Flowchart Astral AI merepresentasikan sistem AI prosedural end-to-end:

**Citra → Segmentasi → Objek → Pengetahuan → Bahasa → Interaksi**

Arsitektur ini menggabungkan Computer Vision deterministik, heuristik klasifikasi, serta Natural Language Understanding berbasis probabilistik ringan.
