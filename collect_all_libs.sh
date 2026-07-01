#!/bin/bash
TARGET=$1
DEST=$2

mkdir -p "$DEST"
# Sadece kendi frameworklerimizi topla, tehlikeli sistem ve arayuz (X11/Wayland/GTK) koprulerini (glib, cairo, pango, gtk, vulkan, avahi vb) filtrele!
ldd "$TARGET" | awk '{print $3}' | grep -v '^$' | grep -E 'Qt|av|sw|ndi|SDL|crypto|ssl' | grep -v -E 'cairo|pango|gtk|gdk|glib|gio|gobject|harfbuzz|vulkan|avahi|json-glib|graphene|libc\+\+' | while read lib; do
    if [ -f "$lib" ]; then
        cp -Lv "$lib" "$DEST/"
        # Eğer ldd'nin okuduğu dosya bir kısayolsa (örn: libQt6Gui.so.6), onu da asıl dosyaya işaret edecek şekilde oluşturmak yerine DİREKT kopyala
        REAL_LIB=$(readlink -f "$lib")
        LIB_NAME=$(basename "$REAL_LIB")
        if [ "$REAL_LIB" != "$lib" ]; then
            SYM_NAME=$(basename "$lib")
            if [ ! -f "$DEST/$SYM_NAME" ]; then
                cp -f "$DEST/$LIB_NAME" "$DEST/$SYM_NAME"
            fi
        fi
    fi
done
