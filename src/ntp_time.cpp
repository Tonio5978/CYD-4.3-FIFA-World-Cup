#include "ntp_time.h"
#include "../include/config.h"
#include "../include/screens.h"

static bool _synced = false;

// French month names
static const char* MONTHS_FR[] = {
    "Jan","Fev","Mar","Avr","Mai","Juin",
    "Juil","Aou","Sep","Oct","Nov","Dec"
};

void NtpTime::begin() {
    configTzTime(NTP_TIMEZONE, NTP_SERVER);
    struct tm t;
    _synced = getLocalTime(&t, 8000);
    gCtx.ntpSynced = _synced;
    if (_synced) {
        DBG("[NTP] Synced – %02d:%02d:%02d\n", t.tm_hour, t.tm_min, t.tm_sec);
    } else {
        DBG("[NTP] Sync failed\n");
    }
}

void NtpTime::loop() {
    uint32_t now = millis();
    if (now - gCtx.lastNtpSync > NTP_UPDATE_INTERVAL_MS) {
        gCtx.lastNtpSync = now;
        struct tm t;
        _synced = getLocalTime(&t, 5000);
        gCtx.ntpSynced = _synced;
        DBG("[NTP] Periodic sync %s\n", _synced ? "OK" : "FAILED");
    }

    // Update shared localTime every second
    if (now - gCtx.lastClockDraw >= 1000) {
        gCtx.lastClockDraw = now;
        getLocalTime(&gCtx.localTime, 0);
    }
}

bool NtpTime::isSynced() {
    return _synced;
}

bool NtpTime::getLocalTime(struct tm& t) {
    return ::getLocalTime(&t, 0);
}

String NtpTime::formatTime(const struct tm& t) {
    char buf[6];
    snprintf(buf, sizeof(buf), "%02d:%02d", t.tm_hour, t.tm_min);
    return String(buf);
}

String NtpTime::formatDate(const struct tm& t) {
    char buf[24];
    snprintf(buf, sizeof(buf), "%d %s %d",
             t.tm_mday,
             MONTHS_FR[t.tm_mon],
             t.tm_year + 1900);
    return String(buf);
}

// Equivalent portable de timegm() (absent de cette newlib) : convertit un
// struct tm interprete en UTC vers un time_t (algorithme days_from_civil).
static time_t timegmPortable(int year, int mon1_12, int day,
                             int hour, int min, int sec) {
    int y = year - (mon1_12 <= 2);
    int era = (y >= 0 ? y : y - 399) / 400;
    unsigned yoe = (unsigned)(y - era * 400);
    unsigned doy = (153 * (mon1_12 + (mon1_12 > 2 ? -3 : 9)) + 2) / 5 + day - 1;
    unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    long long days = (long long)era * 146097 + (long long)doe - 719468;
    return (time_t)(days * 86400LL + hour * 3600 + min * 60 + sec);
}

// Parse "YYYY-MM-DDTHH:MMZ" (UTC) -> struct tm dans le fuseau local.
// localtime_r applique NTP_TIMEZONE (positionne par configTzTime, DST inclus).
static bool isoToLocalTm(const String& iso, struct tm& local) {
    if (iso.length() < 16) return false;
    time_t t = timegmPortable(
        iso.substring(0, 4).toInt(),
        iso.substring(5, 7).toInt(),
        iso.substring(8, 10).toInt(),
        iso.substring(11, 13).toInt(),
        iso.substring(14, 16).toInt(),
        0);
    localtime_r(&t, &local);
    return true;
}

String NtpTime::localTimeFromIso(const String& isoUtc) {
    struct tm lt;
    if (!isoToLocalTm(isoUtc, lt)) return "--:--";
    return formatTime(lt);
}

String NtpTime::localDateFromIso(const String& isoUtc) {
    struct tm lt;
    if (!isoToLocalTm(isoUtc, lt)) return "";
    return formatDate(lt);
}

time_t NtpTime::isoToEpochUtc(const String& iso) {
    if (iso.length() < 16) return 0;
    return timegmPortable(
        iso.substring(0, 4).toInt(),
        iso.substring(5, 7).toInt(),
        iso.substring(8, 10).toInt(),
        iso.substring(11, 13).toInt(),
        iso.substring(14, 16).toInt(),
        0);
}
