#include "storage.h"
#include <LittleFS.h>
#include <PNGdec.h>
#include "../include/config.h"

// Drapeaux stockes en Flash interne (LittleFS) au lieu de la carte SD.
// L'image se televerse avec : pio run --target uploadfs
// L'arborescence du dossier data/ (data/flags/...) devient la racine FS
// (/flags/...), donc les chemins ne changent pas.

static bool _ready = false;

// Shared PNG decoder state.
// IMPORTANT : l'objet PNG de PNGdec fait ~40 Ko (fenetre zlib 32 Ko +
// buffers). Il DOIT etre statique, jamais sur la pile : la pile de
// loopTask ne fait que 8 Ko -> stack overflow immediat sinon, d'autant
// que pngDraw() est rappele pour chaque ligne de l'image.
static PNG       _png;
static uint16_t* _pngDstBuf  = nullptr;
static int       _pngDstW    = 0;
static int       _pngDstH    = 0;
static File      _pngFile;

// PNGdec callbacks -------------------------------------------------------
static void* pngOpen(const char* path, int32_t* size) {
    _pngFile = LittleFS.open(path);
    if (!_pngFile) return nullptr;
    *size = _pngFile.size();
    return &_pngFile;
}
static void pngClose(void* handle) {
    if (handle) ((File*)handle)->close();
}
static int32_t pngRead(PNGFILE* handle, uint8_t* buf, int32_t len) {
    return _pngFile.read(buf, len);
}
static int32_t pngSeek(PNGFILE* handle, int32_t pos) {
    return _pngFile.seek(pos) ? pos : -1;
}

static int pngDraw(PNGDRAW* pDraw) {
    // PNGdec : retourner NON-zero (1) pour continuer le decodage, 0 = stop
    // (PNG_QUIT_EARLY). La convention etait inversee -> decode interrompu.
    if (!_pngDstBuf) return 0;   // vraie erreur -> arreter
    if (pDraw->y < _pngDstH) {
        uint16_t* row = _pngDstBuf + pDraw->y * _pngDstW;
        // BIG_ENDIAN : ordre d'octets attendu par LovyanGFX pushImage.
        // LITTLE_ENDIAN inversait les octets RGB565 -> couleurs faussees
        // (rouge<->bleu, vert<->rouge).
        _png.getLineAsRGB565(pDraw, row, PNG_RGB565_BIG_ENDIAN, 0xFFFFFF);
    }
    return 1;                    // continuer
}
// ------------------------------------------------------------------------

bool Storage::begin() {
    if (!LittleFS.begin()) {
        DBG("[FS] LittleFS mount failed (image televersee ? pio run -t uploadfs)\n");
        _ready = false;
        return false;
    }
    DBG("[FS] LittleFS monte – %u KB / %u KB utilises\n",
        (unsigned)(LittleFS.usedBytes() / 1024),
        (unsigned)(LittleFS.totalBytes() / 1024));
    _ready = true;
    return true;
}

bool Storage::isReady() { return _ready; }

bool Storage::loadImageRGB565(const String& path, uint16_t* outBuf, int w, int h) {
    if (!_ready) return false;

    if (!LittleFS.exists(path)) {
        DBG("[FS] Image not found: %s\n", path.c_str());
        return false;
    }

    int rc = _png.open(path.c_str(), pngOpen, pngClose, pngRead, pngSeek, pngDraw);
    if (rc != PNG_SUCCESS) {
        DBG("[FS] PNG open error %d: %s\n", rc, path.c_str());
        return false;
    }

    // PNGdec decode A LA RESOLUTION NATIVE du fichier (pas de scaling).
    // Un meme PNG (64x48) sert a plusieurs tailles d'affichage (32x24,
    // 64x48...). Si la taille demandee != native, on decode dans un
    // buffer temporaire PSRAM puis on rééchantillonne (plus proche
    // voisin) vers outBuf. Sinon, decode direct (chemin rapide).
    int nativeW = _png.getWidth();
    int nativeH = _png.getHeight();

    if (nativeW == w && nativeH == h) {
        _pngDstBuf = outBuf;
        _pngDstW   = w;
        _pngDstH   = h;
        rc = _png.decode(nullptr, 0);
        _png.close();
        return rc == PNG_SUCCESS;
    }

    uint16_t* tmp = (uint16_t*)heap_caps_malloc((size_t)nativeW * nativeH * 2,
                                                MALLOC_CAP_SPIRAM);
    if (!tmp) {
        DBG("[FS] No PSRAM for %dx%d scale buffer (%s)\n", nativeW, nativeH,
            path.c_str());
        _png.close();
        return false;
    }

    _pngDstBuf = tmp;
    _pngDstW   = nativeW;
    _pngDstH   = nativeH;
    rc = _png.decode(nullptr, 0);
    _png.close();

    if (rc == PNG_SUCCESS) {
        // Rééchantillonnage plus proche voisin native -> w x h
        for (int dy = 0; dy < h; dy++) {
            int sy = dy * nativeH / h;
            const uint16_t* srcRow = tmp + (size_t)sy * nativeW;
            uint16_t*       dstRow = outBuf + (size_t)dy * w;
            for (int dx = 0; dx < w; dx++) {
                dstRow[dx] = srcRow[dx * nativeW / w];
            }
        }
    }

    heap_caps_free(tmp);
    return rc == PNG_SUCCESS;
}

bool Storage::loadFlagRGB565(const String& teamCode, uint16_t* outBuf, int w, int h,
                             bool large) {
    String path = large
        ? String(FLAG_DIR) + "/large/" + teamCode + ".png"
        : String(FLAG_DIR) + "/" + teamCode + ".png";
    return loadImageRGB565(path, outBuf, w, h);
}

bool Storage::exists(const String& path) {
    return _ready && LittleFS.exists(path);
}
