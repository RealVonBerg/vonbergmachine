# VONBERGMACHINE

Phonk / Brezilya funk tarzında tam uzunlukta davul dizilimi (arrangement) üreten VST3 instrument.
Gönderilen 5 referans parça (ROCKY I–V) analiz edilerek modellendi.

## Derleme (GitHub Actions → Windows VST3)

1. GitHub'da yeni bir repo aç (örn. `vonbergmachine`).
2. Bu klasörün içeriğini repoya yükle. `.github/workflows/build.yml` dosyası da yüklenmeli.
3. **Actions** sekmesinde "Build VONBERGMACHINE" çalışır (~10–15 dk, JUCE 8.0.4'ü kendisi indirir).
4. Bitince **Artifacts** kısmından `VONBERGMACHINE-Windows-VST3` zip'ini indir.
5. `VONBERGMACHINE.vst3` klasörünü `C:\Program Files\Common Files\VST3\` içine kopyala.
6. FL Studio → Options → Manage plugins → Find plugins. Plugin, **Generators** altında görünür.

## Kullanım

| Bölüm | Ne yapar |
|---|---|
| **< 001 / 500 >** | 500 hazır pattern (fare tekerleği de çalışır). Her biri intro, drop'lar, geçişler ve outro'dan oluşan tam bir dizilim. |
| **RANDOM** | Seçili stilde sıfırdan yeni bir dizilim üretir (500 ile sınırlı değil). |
| **STYLE** | AUTO veya ROCKY I (100) / II (130) / III (123) / IV (124) / V (117). Parantez içindekiler referans BPM'ler. |
| **LENGTH** | 32 / 48 / 64 / 96 bar. |
| **808 KEY** | AUTO, stilin referanstaki tonunu kullanır. |
| **FREE RUN** | FL'de pattern modunda çalışırken aç. Kapalıyken dizilim, şarkı pozisyonunu takip eder (Song mode'da playlist'e göre). |
| **PREVIEW ▶** | Host çalmadan dizilimi dinletir. |
| **ARRANGEMENT** | Bölümler, otomatik LP/HP filtre eğrileri, reverb throw'lar, riser'lar ve reverb kuyruğunun kesildiği noktalar (▼). Bir bara tıklarsan o bar düzenlenir. |
| **Step grid** | Seçili barın 16 adımı. Tıklayınca vuruş ekler veya siler. Noktalı hücreler roll'dur (snare/hat roll). 808 notaları uzunluk ve pitch ile gösterilir. |
| **KIT** | Her satıra **FL Studio browser'ından sample sürükle-bırak**. Tıklayınca dosya seçilir, sağ tık → built-in sese döner. M = mute, S = solo. |
| **VOL / TUNE / DECAY / REVERB** | Satır başına ses, ton, uzunluk (808'de release) ve reverb send. |
| **GROOVE** | Swing, humanize, 808 glide süresi. |
| **SIDECHAIN** | Kick ve stomp 808'i pompalar. Depth/Release ayarlanabilir, grafikte görünür. |
| **SPACE** | Reverb boyutu ve miktarı. |
| **TRANSITIONS** | Filtre hareketlerinin ve riser'ın miktarı. ON/OFF tüm otomatik geçişleri açar/kapatır. |
| **MASTER** | Drive (saturasyon/clip) ve çıkış seviyesi. |
| **DRAG MIDI / DRAG AUDIO** | Dizilimi .mid veya filtreler + reverb dahil render edilmiş .wav olarak FL playlist'e sürükle. Tıklarsan dosyanın bulunduğu klasör açılır. |

### MIDI girişi (canlı çalma ve export edilen MIDI'yi geri çalma)
MIDI nota numaraları: 36 Kick · 38 Snare · 39 Clap · 42 Hat · 46 Open Hat · 45 Perc · 41 Stomp · 49 Impact · **48 ve üstü = 808 (kromatik, 48 = C kök)**

## Referans analizinden modellenenler
- **Groove:** Kick 1 ve 3'te ya da 4/4 stomp. Clap/snare 2 ve 4'te. Perc "tamborzão" vuruşları 16'lık grid'de 3, 8, 11 ve 16. adımlarda.
- **808:** Kök ses ~31–40 Hz, distorsiyonlu. Dört çeşit pattern ailesi var: uzun-boşluklu, rolling, bounce ve sustain. Slide'lar, oktav atlamaları ve dive'lar (intro'da +24 → 0) dahil.
- **Sidechain:** Kick'ten sonraki 16'lıkta 808'de ~15 dB'lik pompalama.
- **Geçişler:**
  - Tam stop (sessizlik, ya da stomp + reverb throw)
  - Highpass break (~800–1100 Hz, reverb ve genişlik artar, sonunda 808 geri kayarak girer)
  - Lowpass break (~250 Hz'den açılan sweep, crash ile başlar, son vuruşta ses düşer)
  - Build (16'lık → 32'lik → 64'lük hızlanan snare roll, velocity ve pitch yükselir; noise riser ve HP sweep eşlik eder)
- **Drop girişleri:** Reverb kuyruğu kesilir, impact vurur.

## Proje yapısı
- `Source/engine/` — JUCE'suz çekirdek: generator, built-in kit sentezi, sampler, FX
- `Source/PluginProcessor.*`, `Source/PluginEditor.*` — JUCE plugin ve arayüz
- `tools/render_test.cpp` — motoru JUCE olmadan test eden offline render aracı
