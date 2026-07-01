#!/bin/bash
TARGET_DIR="$1"
cd "$TARGET_DIR" || exit 1

# Tehlikeli sistem kütüphanelerini build klasöründen zorla sil (Crash engellemek icin)
rm -f libcairo* libgtk* libvulkan* libpango* libgdk* libglib* libgio* libgobject* libgraphene* libharfbuzz* libjson-glib* libavahi*

for file in *.so*; do
    if [ -f "$file" ] && [ ! -L "$file" ]; then
        SONAME=$(readelf -d "$file" | grep SONAME | awk '{print $5}' | tr -d '[]')
        if [ -n "$SONAME" ] && [ "$SONAME" != "$file" ]; then
            cp -f "$file" "$SONAME"
        fi
        
        # Unversioned (.so) dosyasını da kopyala
        BASENAME=$(echo "$file" | sed -E 's/(\.so).*$/\1/')
        if [ -n "$BASENAME" ] && [ "$BASENAME" != "$file" ] && [ "$BASENAME" != "$SONAME" ]; then
            cp -f "$file" "$BASENAME"
        fi
    fi
done

# qt.conf dosyasını oluşturarak Qt eklentilerinin doğru klasörden okunmasını sağla
cat > qt.conf <<EOF
[Paths]
Prefix = .
Plugins = .
EOF

# Tüm eklenti (plugin) kütüphanelerine patchelf ile RPATH yazarak üst klasördeki (yani binary'nin yanındaki) kütüphaneleri görmelerini sağla
if [ -d "platforms" ]; then
    echo "Platform eklentilerinin RPATH'leri ayarlaniyor..."
    for lib in platforms/*.so* imageformats/*.so* iconengines/*.so* xcbglintegrations/*.so* wayland-graphics-integration-client/*.so*; do
        [ -f "$lib" ] && [ ! -L "$lib" ] || continue
        # Eklentiler genellikle ana dizine göre 1 seviye derindedir.
        # Bu yüzden RPATH'e $ORIGIN/.. ekliyoruz.
        patchelf --force-rpath --set-rpath '$ORIGIN:$ORIGIN/..' "$lib" 2>/dev/null
    done
fi

# Ana klasördeki tüm kütüphanelerin RPATH'ini $ORIGIN olarak ayarla
echo "Kütüphanelerin RPATH'leri ayarlaniyor..."
for lib in *.so*; do
    [ -f "$lib" ] && [ ! -L "$lib" ] || continue
    patchelf --force-rpath --set-rpath '$ORIGIN' "$lib" 2>/dev/null
done

# Qt Creator'un (CMake) kullanıcının yerine otomatik olarak ikona sahip bir Linux başlatıcı (.desktop) oluşturmasını sağla
cat > Now2Play.desktop <<EOF
[Desktop Entry]
Name=Now2Play
Comment=Now2Play Media Player
Exec=$(pwd)/now2play-lite
Icon=$(pwd)/now2play.png
Terminal=false
Type=Application
EOF

# Masaüstü kısayoluna çift tıklanabilmesi için çalışma (Executable) izni ver
chmod +x Now2Play.desktop

