# 🚀 SMV DAQ System
**Sistem Akuisisi Data & Telemetri Real-Time untuk Kendaraan Ultra-Efisien**

Selamat datang di repositori **SMV DAQ (Data Acquisition) System**! 

Jika Anda sedang mengembangkan kendaraan listrik masa depan, purwarupa *Fuel Cell*, atau sedang bersaing di ajang bergengsi seperti **Shell Eco-marathon**, Anda pasti tahu satu rahasia besar: **"Data adalah kunci kemenangan."** 

SMV DAQ dirancang khusus sebagai "otak analitik" kendaraan Anda. Sistem ini tidak hanya membaca sensor, tetapi juga **memahami dinamika berkendara secara cerdas** dan mengirimkan metrik paling krusial secara *real-time* kepada pembalap maupun kru di *paddock*.

---

## 💡 Mengapa Anda Membutuhkan SMV DAQ?

Dalam balapan efisiensi, margin kemenangan seringkali hanya terpaut beberapa Joule atau detik. SMV DAQ memberi Anda keunggulan kompetitif yang tidak tertandingi:

### 1. 📈 Pemantauan Daya Tingkat Presisi (High-Precision Power Metering)
Dilengkapi ADC 16-bit (**ADS1115**) dan penguat arus AD8418, sistem ini memantau konsumsi energi kendaraan Anda (Tegangan, Arus, Daya dalam Watt, dan Energi dalam kWh) secara *real-time*. Anda bisa melihat lonjakan daya sekecil apapun!

### 2. 🧠 Analisis Strategi Berkendara (Drive State & Gear Detection)
Kapan harus menekan gas (*Pulling*) dan kapan harus membiarkan mobil meluncur (*Gliding*)?
Algoritme cerdas kami secara otomatis mendeteksi status *Pull/Glide* serta mendeteksi posisi gigi (Gear) dengan membandingkan rasio RPM Motor dan RPM Roda. Data ini sangat berharga untuk mengevaluasi strategi *driver* di lintasan!

### 3. 📡 Konektivitas Ekosistem Tanpa Batas (CAN Bus, BLE, & WiFi)
Tidak ada lagi data yang terperangkap di dalam mobil:
- **Bluetooth Low Energy (BLE):** Memancarkan data langsung ke *Dashboard* HP/Tablet Android Pembalap dengan latensi sangat rendah (**20 Hz** untuk data dinamis seperti Daya & Kecepatan).
- **CAN Bus (TWAI):** Terintegrasi langsung dengan VESC (Motor Controller) atau sistem Fuel Cell standar industri pada kecepatan 500 kbps.
- **WiFi & HTTP Post:** (Opsional) Mengirim data telemetri ke *Cloud Server* untuk analisis kru di *paddock*.

### 4. 📍 Pemetaan Dinamika & Geospasial
Membawa modul **GPS Presisi** dipadukan dengan modul IMU 6-Axis **MPU6500**. Sistem tidak hanya tahu *di mana* kendaraan berada, tetapi juga *bagaimana* kemiringannya (elevasi jalan), kecepatannya, serta akselerasi G-Force saat bermanuver.

### 5. ⚙️ Zero Bottleneck (Berbasis FreeRTOS)
Arsitektur *software* dibangun di atas **ESP32 Dual-Core dengan FreeRTOS**. Sensor membaca data frekuensi tinggi (10ms) di satu *core*, sementara transmisi BLE/CAN dan komputasi berat berjalan di *core* lainnya. Hasilnya? **Sistem yang anti-lag dan sangat andal (Reliable).**

---

## 🛠️ Spesifikasi Teknis (Under the Hood)

| Komponen / Modul | Deskripsi | Protokol |
| :--- | :--- | :--- |
| **Microcontroller** | ESP32 (Dual Core) dengan sistem operasi FreeRTOS | - |
| **IMU Sensor** | MPU6500 (Akselerometer & Giroskop 6-axis) | I2C (400 kHz) |
| **Power Sensor** | ADS1115 (16-bit ADC) untuk Teganangan & Shunt Arus | I2C (400 kHz) |
| **Environment** | WSEN_HIDS (Suhu & Kelembaban Ambient) | I2C (400 kHz) |
| **Speed Sensor** | Pembacaan pulsa *Hall Effect* / Roda (Interrupt-based) | GPIO Interrupt |
| **GPS Module** | Pelacakan geolokasi (GPRMC, GPGGA, GPVTG) | UART (9600 bps) |
| **Motor/FC Comm.** | Komunikasi 2-arah ke VESC / Sistem Fuel Cell | CAN Bus (500 kbps)|
| **Wireless Comm.** | NimBLE (ESP32 Bluetooth) MTU-Optimized | BLE (2.4 GHz) |

---

## 📐 Arsitektur Transmisi BLE (Optimasi Bandwidth)

Sistem kami mengelompokkan data menjadi beberapa *Characteristic* agar aplikasi *Dashboard* klien (*Android/iOS*) tidak *overload*. Frekuensi pengiriman (Refresh Rate) disesuaikan secara dinamis:
- 🚀 **Fast (20 Hz):** Daya (Voltage, Current, Power), Kecepatan (Speed), dan IMU.
- 🚶 **Medium (5 Hz):** Kalkulasi Lanjutan (Drive State, Pull/Glide Timer, Gear Ratio).
- 🐢 **Slow (1 Hz):** GPS, Suhu Lingkungan (Environment), dan Status Sistem (Uptime, Free Heap).

---

## 🚀 Mulai Menggunakan (Getting Started)

Bagi tim *engineer* Anda, setup sistem ini sangatlah mudah:

1. **Kloning Repositori & Buka dengan PlatformIO:**
   ```bash
   git clone https://github.com/your-username/SMV_DAQ.git
   ```
   Buka *workspace* menggunakan ekstensi PlatformIO di VS Code.

2. **Instalasi Dependencies:**
   PlatformIO akan otomatis mengunduh *library* yang diperlukan (lihat `platformio.ini` Anda), antara lain:
   - `MPU9250_WE` (Akses MPU6500)
   - `Adafruit_ADS1X15`
   - `ArduinoJson`

3. **Konfigurasi Kalibrasi (Penting!):**
   Salin file `include/config_template.h` menjadi `include/config.h` (file ini wajib diabaikan dari git / di masukkan ke `.gitignore`). 
   Sesuaikan nilai kalibrasi *hardware* khusus kendaraan Anda:
   ```cpp
   // Contoh Kalibrasi di config.h
   #define VDIV_R1 39000.0f               // Nilai resistor pembagi tegangan
   #define SHUNT_RESISTANCE 0.001f        // Nilai Shunt
   #define WHEEL_CIRCUMFERENCE_MM 1540.0f // Keliling roda
   ```

4. **Build & Upload:**
   Sambungkan ESP32 Anda, lalu jalankan `PlatformIO: Upload`.

---

## 📊 Status Debugging Fleksibel

Ingin melacak *bug* atau melihat *raw data*? Kami menyediakan `debug_logging.h`. Anda dapat menghidupkan dan mematikan modul log secara instan tanpa membebani sistem pembacaan yang sedang berjalan.

---

## 🤝 Lisensi & Kontribusi

Dikembangkan dengan dedikasi untuk efisiensi energi. Kami sangat terbuka untuk *Pull Request* dan peningkatan kode dari tim *developer* lainnya. Silakan buat *Issue* jika menemukan kendala.

> **SMV DAQ: Empowering Your Vehicle, Winning Your Race.** 🏆