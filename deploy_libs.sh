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

# Qt Creator'un (CMake) kullanıcının yerine otomatik olarak ikona sahip bir Linux başlatıcı (.desktop) oluşturmasını sağla
cat > Now2Play.desktop <<EOF
[Desktop Entry]
Name=Now2Play
Comment=Now2Play Media Player
Exec=$(pwd)/now2play
Icon=$(pwd)/now2play.png
Terminal=false
Type=Application
EOF

# Masaüstü kısayoluna çift tıklanabilmesi için çalışma (Executable) izni ver
chmod +x Now2Play.desktop
