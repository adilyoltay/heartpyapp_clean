# 📊 HeartPy C++ API Metrikleri Detaylı Raporu

## 📌 Özet
HeartPy C++ API, kalp hızı değişkenliği (HRV) analizi için kapsamlı bir metrik seti sunar. Bu rapor, mevcut tüm metrikleri, açıklamalarını ve pratik kullanım alanlarını detaylandırır.

---

## 🎯 Temel Metrikler (Basic Metrics)

### 1. **BPM (Beats Per Minute)**
- **Tip:** `double`
- **Açıklama:** Dakikadaki kalp atım sayısı
- **Aralık:** Tipik olarak 40-200 bpm
- **Kullanım Alanları:**
  - Genel sağlık durumu değerlendirmesi
  - Egzersiz yoğunluğu takibi
  - Stres seviyesi tespiti
  - Uyku kalitesi analizi

### 2. **IBI (Inter-Beat Intervals)**
- **Tip:** `std::vector<double> ibiMs`
- **Açıklama:** Ardışık kalp atımları arasındaki zaman aralıkları (milisaniye)
- **Kullanım Alanları:**
  - HRV hesaplamalarının temel verisi
  - Aritmi tespiti
  - Otonom sinir sistemi değerlendirmesi

### 3. **RR List**
- **Tip:** `std::vector<double> rrList`
- **Açıklama:** Temizlenmiş ve filtrelenmiş RR intervalleri
- **Kullanım Alanları:**
  - Güvenilir HRV analizi
  - Zaman domain metrikleri hesaplama
  - Frekans domain analizi

### 4. **Peak Lists**
- **Tip:** 
  - `std::vector<int> peakList` - Temiz peak indeksleri
  - `std::vector<int> peakListRaw` - Ham peak indeksleri
  - `std::vector<int> binaryPeakMask` - Kabul/red maskesi
- **Açıklama:** Tespit edilen R-peak'lerin konumları
- **Kullanım Alanları:**
  - Sinyal kalitesi değerlendirmesi
  - Artefakt tespiti
  - Beat segmentasyonu

### 5. **Peak Timestamps**
- **Tip:** `std::vector<double> peakTimestamps`
- **Açıklama:** Peak'lerin zaman damgaları (saniye)
- **Kullanım Alanları:**
  - Gerçek zamanlı görselleştirme
  - Senkronizasyon
  - Haptic feedback zamanlaması

---

## 📈 Zaman Domain Metrikleri (Time Domain)

### 1. **SDNN (Standard Deviation of NN intervals)**
- **Tip:** `double`
- **Birim:** Milisaniye
- **Açıklama:** Tüm normal RR intervallerinin standart sapması
- **Normal Aralık:** 50-100 ms
- **Kullanım Alanları:**
  - Genel HRV değerlendirmesi
  - Uzun dönem HRV analizi
  - Kardiyovasküler sağlık göstergesi
  - Stres direnci ölçümü

### 2. **RMSSD (Root Mean Square of Successive Differences)**
- **Tip:** `double`
- **Birim:** Milisaniye
- **Açıklama:** Ardışık RR farkların karelerinin ortalamasının karekökü
- **Normal Aralık:** 20-50 ms
- **Kullanım Alanları:**
  - Parasempatik aktivite göstergesi
  - Kısa dönem HRV analizi
  - Meditasyon/rahatlama değerlendirmesi
  - Uyku kalitesi analizi

### 3. **SDSD (Standard Deviation of Successive Differences)**
- **Tip:** `double`
- **Birim:** Milisaniye
- **Açıklama:** Ardışık RR farklarının standart sapması
- **Kullanım Alanları:**
  - RMSSD'ye benzer parasempatik değerlendirme
  - Kısa dönem değişkenlik analizi

### 4. **pNN20 & pNN50**
- **Tip:** `double`
- **Birim:** Yüzde (%)
- **Açıklama:** 
  - pNN20: Ardışık RR farkı >20ms olan beat yüzdesi
  - pNN50: Ardışık RR farkı >50ms olan beat yüzdesi
- **Kullanım Alanları:**
  - Parasempatik tonus değerlendirmesi
  - Yaşa bağlı HRV değişimleri
  - Otonom disfonksiyon tespiti

### 5. **NN20 & NN50**
- **Tip:** `double`
- **Birim:** Sayı
- **Açıklama:** pNN20/pNN50'nin mutlak sayı karşılıkları
- **Kullanım Alanları:**
  - İstatistiksel analiz
  - Veri kalitesi değerlendirmesi

### 6. **MAD (Median Absolute Deviation)**
- **Tip:** `double`
- **Birim:** Milisaniye
- **Açıklama:** RR intervallerinin medyan mutlak sapması
- **Kullanım Alanları:**
  - Outlier'lara dayanıklı değişkenlik ölçümü
  - Artefakt varlığında güvenilir analiz
  - Non-parametrik HRV değerlendirmesi

---

## 🎨 Poincaré Analizi

### 1. **SD1 (Short-term variability)**
- **Tip:** `double`
- **Birim:** Milisaniye
- **Açıklama:** Poincaré plot'ta kısa dönem değişkenlik
- **Kullanım Alanları:**
  - Anlık kalp hızı değişkenliği
  - Parasempatik aktivite
  - Solunum etkisi analizi

### 2. **SD2 (Long-term variability)**
- **Tip:** `double`
- **Birim:** Milisaniye
- **Açıklama:** Poincaré plot'ta uzun dönem değişkenlik
- **Kullanım Alanları:**
  - Uzun dönem HRV trendi
  - Sempatik + parasempatik aktivite
  - Sirkadiyen ritm analizi

### 3. **SD1/SD2 Ratio**
- **Tip:** `double`
- **Açıklama:** Kısa/uzun dönem değişkenlik oranı
- **Kullanım Alanları:**
  - Otonom denge değerlendirmesi
  - Kardiyovasküler risk tahmini
  - Egzersiz adaptasyonu takibi

### 4. **Ellipse Area**
- **Tip:** `double`
- **Birim:** ms²
- **Açıklama:** Poincaré elips alanı (π × SD1 × SD2)
- **Kullanım Alanları:**
  - Toplam HRV göstergesi
  - Görsel HRV değerlendirmesi
  - Karşılaştırmalı analizler

---

## 🌊 Frekans Domain Metrikleri

### 1. **VLF (Very Low Frequency)**
- **Tip:** `double`
- **Birim:** ms²
- **Frekans Aralığı:** 0.0033 - 0.04 Hz
- **Kullanım Alanları:**
  - Termoregülasyon
  - Hormonal düzenlemeler
  - Uzun dönem düzenleyici mekanizmalar
  - En az 5 dakikalık kayıt gerektirir

### 2. **LF (Low Frequency)**
- **Tip:** `double`
- **Birim:** ms²
- **Frekans Aralığı:** 0.04 - 0.15 Hz
- **Kullanım Alanları:**
  - Sempatik + parasempatik aktivite
  - Baroreseptör aktivitesi
  - Kan basıncı düzenlemesi
  - Mental stres değerlendirmesi

### 3. **HF (High Frequency)**
- **Tip:** `double`
- **Birim:** ms²
- **Frekans Aralığı:** 0.15 - 0.4 Hz
- **Kullanım Alanları:**
  - Parasempatik (vagal) aktivite
  - Solunum etkisi (RSA)
  - Rahatlama/meditasyon değerlendirmesi
  - Akut stres yanıtı

### 4. **LF/HF Ratio**
- **Tip:** `double`
- **Açıklama:** Sempatovagal denge göstergesi
- **Normal Aralık:** 1.5 - 2.0
- **Kullanım Alanları:**
  - Otonom sinir sistemi dengesi
  - Stres seviyesi değerlendirmesi
  - Yorgunluk tespiti
  - Overtraining sendromu

### 5. **Total Power**
- **Tip:** `double`
- **Birim:** ms²
- **Açıklama:** VLF + LF + HF toplam güç
- **Kullanım Alanları:**
  - Genel otonom aktivite
  - Adaptasyon kapasitesi
  - Sağlık durumu göstergesi

### 6. **LF Norm & HF Norm**
- **Tip:** `double`
- **Birim:** n.u. (normalized units)
- **Formül:** 
  - LF_norm = LF / (LF + HF) × 100
  - HF_norm = HF / (LF + HF) × 100
- **Kullanım Alanları:**
  - VLF etkisinden arındırılmış analiz
  - Karşılaştırmalı çalışmalar
  - Pozisyon değişikliği etkileri

---

## 🌬️ Solunum Analizi

### **Breathing Rate**
- **Tip:** `double`
- **Birim:** Nefes/dakika
- **Açıklama:** HRV'den türetilen solunum hızı
- **Normal Aralık:** 12-20 nefes/dk
- **Kullanım Alanları:**
  - Solunum patern analizi
  - Meditasyon/yoga değerlendirmesi
  - Uyku apnesi tespiti
  - Anksiyete değerlendirmesi
  - Biofeedback uygulamaları

---

## 🎯 Kalite Metrikleri (Quality Metrics)

### Temel Kalite Göstergeleri

#### 1. **Total Beats & Rejected Beats**
- **Tip:** `int`
- **Açıklama:** Toplam ve reddedilen beat sayıları
- **Kullanım Alanları:**
  - Veri güvenilirliği değerlendirmesi
  - Artefakt oranı hesaplama

#### 2. **Rejection Rate**
- **Tip:** `double`
- **Birim:** 0-1 arası oran
- **Açıklama:** Reddedilen beat oranı
- **Kullanım Alanları:**
  - Sinyal kalitesi göstergesi
  - Analiz güvenilirliği

#### 3. **Good Quality**
- **Tip:** `bool`
- **Açıklama:** Genel kalite değerlendirmesi
- **Kullanım Alanları:**
  - Otomatik kalite kontrolü
  - Veri filtreleme

### Gelişmiş Kalite Metrikleri

#### 4. **SNR (Signal-to-Noise Ratio)**
- **Tip:** `double snrDb`
- **Birim:** Desibel (dB)
- **Açıklama:** Sinyal/gürültü oranı
- **İyi Değer:** > 10 dB
- **Kullanım Alanları:**
  - PPG sinyal kalitesi
  - Hareket artefaktı tespiti
  - Optimal kayıt koşulları değerlendirmesi

#### 5. **Confidence Score**
- **Tip:** `double confidence`
- **Aralık:** 0.0 - 1.0
- **Açıklama:** Genel güven skoru
- **Formül:** SNR × (1 - rejection_rate) × CV_penalty
- **Kullanım Alanları:**
  - UI feedback (renk kodlama)
  - Haptic feedback tetikleme
  - Otomatik veri seçimi

#### 6. **F0 Hz (Fundamental Frequency)**
- **Tip:** `double f0Hz`
- **Birim:** Hz
- **Açıklama:** Tespit edilen temel kalp ritmi frekansı
- **Kullanım Alanları:**
  - Harmonik analiz
  - Doubling/halving tespiti
  - Spektral peak doğrulama

### Harmonik Suppresyon Metrikleri

#### 7. **Doubling Flags**
- **Tipler:**
  - `doublingFlag` - Ana doubling göstergesi
  - `softDoublingFlag` - PSD tabanlı yumuşak tespit
  - `doublingHintFlag` - İpucu seviyesi tespit
  - `hardFallbackActive` - Sert fallback modu
- **Açıklama:** Yanlış frekans katlama tespiti
- **Kullanım Alanları:**
  - Algoritma güvenilirliği
  - Yanlış peak tespiti önleme
  - Adaptif eşik ayarlama

#### 8. **RR Interval Analiz Metrikleri**
- **`rrShortFrac`** - Kısa RR interval oranı
- **`rrLongMs`** - Uzun RR interval değeri (ms)
- **`pairFrac`** - Ardışık çift oranı
- **`pHalfOverFund`** - Yarı/temel frekans güç oranı
- **Kullanım Alanları:**
  - Ritim düzensizliği tespiti
  - Artefakt karakterizasyonu
  - Algoritma ince ayarı

### Adaptif Parametre Metrikleri

#### 9. **Refractory Period**
- **Tip:** `double refractoryMsActive`
- **Birim:** Milisaniye
- **Açıklama:** Aktif refrakter periyod
- **Kullanım Alanları:**
  - Peak tespit güvenilirliği
  - Yanlış pozitif önleme
  - Dinamik eşik adaptasyonu

#### 10. **SNR Warm-up Status**
- **Tipler:**
  - `snrWarmupActive` - Warm-up durumu
  - `snrSampleCount` - Kullanılan örnek sayısı
- **Kullanım Alanları:**
  - İlk veri güvenilirliği
  - Adaptasyon süreci takibi
  - UI feedback zamanlaması

---

## 📊 Segment Analizi

### Binary Segments
- **Tip:** `std::vector<BinarySegment>`
- **İçerik:**
  - `index` - Segment sırası
  - `startBeat` - Başlangıç beat indeksi
  - `endBeat` - Bitiş beat indeksi
  - `totalBeats` - Toplam beat sayısı
  - `rejectedBeats` - Reddedilen beat sayısı
  - `accepted` - Segment kabul durumu
- **Kullanım Alanları:**
  - Zaman bazlı kalite analizi
  - Trend tespiti
  - Aktivite segmentasyonu
  - Artefakt lokalizasyonu

---

## 🔄 Gerçek Zamanlı Streaming Metrikleri

### Waveform Snapshots
- **Tipler:**
  - `waveform_values` - Dalga formu değerleri
  - `waveform_timestamps` - Zaman damgaları
- **Kullanım Alanları:**
  - Gerçek zamanlı görselleştirme
  - Sinyal kalitesi gösterimi
  - Debug ve analiz
  - Kullanıcı feedback

---

## 💡 Pratik Uygulama Önerileri

### 1. **Fitness & Spor Uygulamaları**
- **Kullanılacak Metrikler:**
  - BPM (anlık kalp hızı)
  - RMSSD (toparlanma göstergesi)
  - LF/HF (yorgunluk seviyesi)
  - Confidence (veri güvenilirliği)

### 2. **Stres Yönetimi Uygulamaları**
- **Kullanılacak Metrikler:**
  - SDNN (genel stres direnci)
  - LF/HF ratio (akut stres)
  - Breathing Rate (anksiyete göstergesi)
  - HF Power (rahatlama seviyesi)

### 3. **Meditasyon & Mindfulness**
- **Kullanılacak Metrikler:**
  - HF Power (parasempatik aktivite)
  - Breathing Rate (nefes kontrolü)
  - SD1 (anlık değişkenlik)
  - Confidence (oturum kalitesi)

### 4. **Uyku Takibi**
- **Kullanılacak Metrikler:**
  - RMSSD (uyku kalitesi)
  - VLF (derin uyku)
  - Total Power (uyku evreleri)
  - Rejection Rate (hareket tespiti)

### 5. **Medikal Uygulamalar**
- **Kullanılacak Metrikler:**
  - Tüm zaman domain metrikleri
  - Tüm frekans domain metrikleri
  - Poincaré analizi
  - Detaylı kalite metrikleri
  - Segment analizi

---

## 🔧 Kullanım İpuçları

### Minimum Kayıt Süreleri
- **Ultra-kısa (10-30 sn):** BPM, RMSSD, pNN50
- **Kısa (1-2 dk):** + SDNN, LF, HF
- **Orta (5 dk):** + VLF, Total Power
- **Uzun (24 saat):** Tüm metrikler + circadian analiz

### Kalite Eşikleri
- **Yüksek Kalite:** Confidence > 0.85, SNR > 15 dB
- **Orta Kalite:** Confidence > 0.70, SNR > 10 dB
- **Düşük Kalite:** Confidence < 0.70, SNR < 10 dB

### Artefakt Yönetimi
- Rejection Rate < 0.20 için güvenilir
- MAD kullanarak outlier'lara dayanıklı analiz
- Binary segments ile lokal kalite değerlendirmesi

---

## 📱 React Native Entegrasyonu

### TypeScript Interface
```typescript
export type HeartPyResult = {
  // Tüm C++ metrikleri
  bpm: number;
  sdnn: number;
  rmssd: number;
  // ... diğer metrikler
  quality: QualityInfo;
  segments?: HeartPyResult[];
};
```

### Kullanım Örneği
```typescript
const result = await HeartPy.analyze(ppgSignal, sampleRate, {
  windowSeconds: 12,
  calcFreq: true,
  adaptivePsd: true
});

// Metrik kullanımı
console.log(`Heart Rate: ${result.bpm} BPM`);
console.log(`HRV (RMSSD): ${result.rmssd} ms`);
console.log(`Stress Level: ${result.lfhf}`);
console.log(`Signal Quality: ${result.quality.confidence * 100}%`);
```

---

## 🎯 Sonuç

HeartPy C++ API, kapsamlı metrik setiyle profesyonel kalp hızı değişkenliği analizi için gerekli tüm araçları sağlar. Temel BPM ölçümünden gelişmiş spektral analize, kalite kontrolünden gerçek zamanlı streaming'e kadar geniş bir yelpazede çözümler sunar.

**Öne Çıkan Özellikler:**
- ✅ 40+ farklı metrik
- ✅ Gerçek zamanlı analiz desteği
- ✅ Adaptif kalite kontrolü
- ✅ Harmonik suppresyon
- ✅ Segment bazlı analiz
- ✅ React Native entegrasyonu

Bu metrikler, fitness takibinden medikal uygulamalara, stres yönetiminden uyku analizine kadar geniş bir uygulama yelpazesinde kullanılabilir.
