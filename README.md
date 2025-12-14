# Astral: Interactive AI Computer Vision System

![Language](https://img.shields.io/badge/Language-C-blue.svg)
![Platform](https://img.shields.io/badge/Platform-Windows-lightgrey.svg)
![Dependency](https://img.shields.io/badge/Dependency-ImageMagick-orange.svg)

**Astral** adalah sistem kecerdasan buatan berbasis *Computer Vision* yang dirancang untuk menganalisis citra digital dan berinteraksi dengan pengguna menggunakan bahasa alami (*Natural Language Understanding*/NLU).

Berbeda dengan program pengolahan citra konvensional yang hanya mengeluarkan data mentah, Astral memiliki "kepribadian" unik. Sistem ini dibangun sepenuhnya menggunakan algoritma fundamental (tanpa library AI berat seperti TensorFlow, OpenCV, atau PyTorch), menjadikannya studi kasus mendalam tentang struktur data, algoritma pengolahan citra, dan logika *fuzzy* dari nol.

---

## 🧠 Kapabilitas Utama

Astral dirancang dengan tiga pilar kecerdasan utama:

### 1. Visual Perception (Melihat)
Astral mampu membedah gambar hingga level piksel untuk mengidentifikasi objek:
* **Deteksi Warna:** Mengenali spektrum warna utama (Merah, Hijau, Biru, Kuning) menggunakan konversi ruang warna HSV.
* **Segmentasi Objek:** Menggunakan algoritma *Iterative Flood Fill* (Stack-based) untuk memisahkan objek dari latar belakang.
* **Clustering Ukuran:** Menggunakan *K-Means Clustering* untuk secara dinamis mengkategorikan objek menjadi "Kecil", "Sedang", atau "Besar" berdasarkan konteks gambar.

### 2. Context Awareness (Memahami Konteks)
Tidak hanya melihat objek secara individu, Astral memahami komposisi global:
* **Scene Classification:** Menganalisis distribusi histogram warna dan kepadatan objek untuk menentukan apakah gambar tersebut adalah *Pemandangan Alam*, *Dokumen*, *Ilustrasi*, atau *Campuran*.

### 3. Natural Interaction (Berkomunikasi)
Berinteraksi layaknya *chatbot* cerdas:
* **Fuzzy Logic NLU:** Menggunakan algoritma *Levenshtein Distance* untuk mentoleransi kesalahan ketik (typo) pengguna.
* **Intent Recognition:** Menggunakan pendekatan statistik *Naive Bayes* sederhana untuk menebak maksud pengguna (menghitung, mencari lokasi, meminta deskripsi, dll).
* **Human-like Response:** Menjawab dengan variasi kalimat yang luwes dan tidak kaku.

---

## 🛠️ Arsitektur Teknis

Proyek ini ditulis dalam **Pure C** untuk performa maksimal dan pembuktian konsep algoritma:

| Komponen | Algoritma / Metode yang Digunakan |
| :--- | :--- |
| **Preprocessing** | RGB to HSV Conversion, Noise Reduction (Morphological Dilation) |
| **Object Segmentation** | Iterative Flood Fill (menggunakan alokasi Heap/Stack manual untuk mencegah Stack Overflow) |
| **Classification** | K-Means Clustering (Unsupervised Learning sederhana) |
| **NLP/Text Processing** | Levenshtein Distance (String Matching), Tokenization, Probability scoring |
| **Visualization** | ASCII Art Rendering & Direct Pixel Manipulation (PPM format) |

---

## 📋 Prasyarat Sistem

1.  **Sistem Operasi:** Windows (Menggunakan `windows.h` untuk manajemen konsol dan UI).
2.  **Compiler:** GCC (MinGW atau sejenisnya).
3.  **External Tool:** [ImageMagick](https://imagemagick.org/) (Wajib diinstall dan ditambahkan ke PATH).
    * *Astral menggunakan command `magick` untuk mengonversi format gambar input (JPG/PNG) menjadi format mentah (PPM) agar bisa dibaca oleh kode C.*

---

## 🚀 Cara Penggunaan

### 1. Kompilasi
Jalankan perintah berikut di terminal:
```bash
gcc main.c -o astral
```
### 2. Jalankan Program
Siapkan sebuah gambar (misalnya gambar.jpg) di folder images, lalu jalankan:
```bash
# Mode standar
./astral.exe images/gambar.jpg

# Mode dengan output gambar hasil deteksi & filter area minimal
./astral.exe images/gambar.jpg --with-result --min-area 300
