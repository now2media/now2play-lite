#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <libgen.h>
#include <limits.h>

int main(int argc, char **argv) {
    char path[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", path, sizeof(path)-1);
    if (len != -1) {
        path[len] = '\0';
        char *dir = dirname(path);
        
        // Mevcut LD_LIBRARY_PATH'i al ve klasorumuzu en basa ekle
        char new_ld[PATH_MAX * 2];
        const char *old_ld = getenv("LD_LIBRARY_PATH");
        // Gerekli library path'i ayarla (Ana dizin)
        if (old_ld) {
            snprintf(new_ld, sizeof(new_ld), "%s:%s", dir, old_ld);
        } else {
            snprintf(new_ld, sizeof(new_ld), "%s", dir);
        }
        
        setenv("LD_LIBRARY_PATH", new_ld, 1);
        
        // Bazi sistemlerde Wayland cokerse diye guvenli liman olan X11/XCB'ye zorla
        setenv("QT_QPA_PLATFORM", "xcb", 1);
        
        // Gercek uygulamayi calistir
        char exec_path[PATH_MAX];
        snprintf(exec_path, sizeof(exec_path), "%s/now2play-lite.bin", dir);
        execv(exec_path, argv);
        
        perror("Uygulama calistirilemedi (now2play-lite.bin bulunamadi)");
    }
    return 1;
}
