#ifndef GLOBALPROPERTIES_H
#define GLOBALPROPERTIES_H

class globalProperties
{
public:
    // Durumları tanımlıyoruz
    enum PlayerState {
        Nothing, // Hiçbir şey yüklü değil (Açılış durumu)
        Playing, // Oynatılıyor
        Paused,  // Duraklatıldı
        Stopped  // Durduruldu (Video yüklü ama oynamıyor)
    };

    globalProperties() : selectfile(-1), currentState(Nothing), dropIndicatorRow(-1) {}

    int selectfile;
    int dropIndicatorRow; // Sürükle-bırak sırasında hedef satırı tutar
    PlayerState currentState; // Mevcut durumu burada tutuyoruz
};

#endif // GLOBALPROPERTIES_H