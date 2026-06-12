#pragma once
#include <Arduino.h>
#include <time.h>

namespace NtpTime {
    void begin();                       // configure timezone, first sync
    void loop();                        // periodic re-sync
    bool isSynced();
    bool getLocalTime(struct tm& t);    // returns false if not synced
    String formatTime(const struct tm& t);  // "14:35"
    String formatDate(const struct tm& t);  // "11 Juin 2026"

    // Convertit une date ISO UTC ("2026-06-11T19:00Z") vers le fuseau
    // local configure (NTP_TIMEZONE, DST inclus).
    String localTimeFromIso(const String& isoUtc);  // "21:00" ou "--:--"
    String localDateFromIso(const String& isoUtc);  // "11 Juin 2026" ou ""

    // Date ISO UTC -> epoch UTC (secondes). 0 si invalide. Pour comparer a
    // l'heure courante (time(nullptr)) independamment du fuseau.
    time_t isoToEpochUtc(const String& isoUtc);
}
