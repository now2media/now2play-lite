# now2play-lite

Bu proje, now2play uygulamasının sadeleştirilmiş (lite) sürümüdür.

## SDK Kurulumu

Uygulamanın derlenebilmesi ve çalışabilmesi için gerekli olan SDK kütüphaneleri boyutlarından dolayı Git deposuna dahil edilmemiştir. 

Projeyi derlemeden önce:
1. http://sdk.now2media.com adresinden güncel SDK paketini indirin.
2. Arşiv içerisindeki tüm dosyaları ve `header` klasörünü projenin kök dizininde yer alan `SDK` klasörünün içerisine kopyalayın.

## Derleme ve Çalıştırma

Gerekli kütüphaneleri yerleştirdikten sonra aşağıdaki komutlarla projeyi derleyebilirsiniz:

```bash
cmake -B build -S .
cmake --build build
```
