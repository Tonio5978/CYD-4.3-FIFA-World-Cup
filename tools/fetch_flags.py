#!/usr/bin/env python3
"""
fetch_flags.py  –  Download & resize FIFA World Cup 2026 team flags.

Generates two sizes in the LittleFS image (data/, flashed via `pio run -t uploadfs`):
  data/flags/<CODE>.png          →  64×48 px  (match cards, group tables)
  data/flags/large/<CODE>.png    →  200×150 px (goal popup)

<CODE> = ESPN team abbreviation (the firmware looks flags up by that code).
The 48 teams match the 12 groups A→L of the ESPN standings API.

Source: flagcdn.com (free, no API key needed)

Usage:
    python tools/fetch_flags.py                # all 48 teams
    python tools/fetch_flags.py --team BRA     # single team
    python tools/fetch_flags.py --only-large   # only 200×150 (already have small)
    python tools/fetch_flags.py --only-small   # only 64×48
    python tools/fetch_flags.py --prune        # also delete non-qualified flags
"""

import argparse
import sys
import time
import urllib.request
import urllib.error
from pathlib import Path

try:
    from PIL import Image, ImageOps
except ImportError:
    print("ERROR: Pillow not installed.")
    print("       Run:  pip install Pillow")
    sys.exit(1)

# ---------------------------------------------------------------------------
# Paths (script lives in tools/, so project root is one level up)
# ---------------------------------------------------------------------------
PROJECT_ROOT = Path(__file__).resolve().parent.parent
FLAGS_SM_DIR = PROJECT_ROOT / "data" / "flags"
FLAGS_LG_DIR = PROJECT_ROOT / "data" / "flags" / "large"

SM_W, SM_H = 64, 48       # standard flag size (match cards)
LG_W, LG_H = 200, 150     # large flag size (goal popup)

# ---------------------------------------------------------------------------
# Team list  →  (full name, flagcdn.com ISO code)
#
# Codes = ESPN team["abbreviation"] EXACTEMENT (le firmware cherche les
# drapeaux par ce code). Liste alignee sur l'API ESPN standings, 48 equipes
# reparties dans les 12 groupes A→L de la Coupe du Monde 2026.
# Source de verite :
#   https://site.web.api.espn.com/apis/v2/sports/soccer/fifa.world/standings
#
# flagcdn.com utilise l'ISO 3166-1 alpha-2 minuscule, avec des codes de
# subdivision pour le Royaume-Uni (gb-eng = Angleterre, gb-sct = Ecosse).
# ---------------------------------------------------------------------------
TEAMS: dict[str, tuple[str, str]] = {
    # Groupe A
    "MEX": ("Mexico",              "mx"),
    "CZE": ("Czechia",             "cz"),
    "KOR": ("South Korea",         "kr"),
    "RSA": ("South Africa",        "za"),
    # Groupe B
    "CAN": ("Canada",              "ca"),
    "BIH": ("Bosnia-Herzegovina",  "ba"),
    "SUI": ("Switzerland",         "ch"),
    "QAT": ("Qatar",               "qa"),
    # Groupe C
    "BRA": ("Brazil",              "br"),
    "SCO": ("Scotland",            "gb-sct"),
    "HAI": ("Haiti",               "ht"),
    "MAR": ("Morocco",             "ma"),
    # Groupe D
    "PAR": ("Paraguay",            "py"),
    "TUR": ("Turkiye",             "tr"),
    "AUS": ("Australia",           "au"),
    "USA": ("United States",       "us"),
    # Groupe E
    "ECU": ("Ecuador",             "ec"),
    "GER": ("Germany",             "de"),
    "CIV": ("Ivory Coast",         "ci"),
    "CUW": ("Curacao",             "cw"),
    # Groupe F
    "NED": ("Netherlands",         "nl"),
    "SWE": ("Sweden",              "se"),
    "JPN": ("Japan",               "jp"),
    "TUN": ("Tunisia",             "tn"),
    # Groupe G
    "BEL": ("Belgium",             "be"),
    "IRN": ("Iran",                "ir"),
    "EGY": ("Egypt",               "eg"),
    "NZL": ("New Zealand",         "nz"),
    # Groupe H
    "ESP": ("Spain",               "es"),
    "URU": ("Uruguay",             "uy"),
    "KSA": ("Saudi Arabia",        "sa"),
    "CPV": ("Cape Verde",          "cv"),
    # Groupe I
    "NOR": ("Norway",              "no"),
    "FRA": ("France",              "fr"),
    "SEN": ("Senegal",             "sn"),
    "IRQ": ("Iraq",                "iq"),
    # Groupe J
    "ARG": ("Argentina",           "ar"),
    "AUT": ("Austria",             "at"),
    "ALG": ("Algeria",             "dz"),
    "JOR": ("Jordan",              "jo"),
    # Groupe K
    "COL": ("Colombia",            "co"),
    "POR": ("Portugal",            "pt"),
    "UZB": ("Uzbekistan",          "uz"),
    "COD": ("Congo DR",            "cd"),
    # Groupe L
    "ENG": ("England",             "gb-eng"),
    "CRO": ("Croatia",             "hr"),
    "PAN": ("Panama",              "pa"),
    "GHA": ("Ghana",               "gh"),
}

# ---------------------------------------------------------------------------
# Download helpers
# ---------------------------------------------------------------------------

def _download(iso2: str, dest: Path, retries: int = 3) -> bool:
    """Download flag PNG from flagcdn.com (160 px wide source)."""
    url = f"https://flagcdn.com/w160/{iso2}.png"
    headers = {"User-Agent": "FIFA-WC2026-ESP32-fetcher/1.0"}

    for attempt in range(retries):
        try:
            req = urllib.request.Request(url, headers=headers)
            with urllib.request.urlopen(req, timeout=20) as resp:
                dest.write_bytes(resp.read())
            return True
        except urllib.error.HTTPError as e:
            print(f"\n  HTTP {e.code} for {url}")
            return False
        except Exception as e:
            if attempt < retries - 1:
                time.sleep(1.5)
            else:
                print(f"\n  Download failed after {retries} tries: {e}")
    return False


def _resize(src: Path, dst: Path, w: int, h: int) -> bool:
    """
    Resize to exactly w×h using ImageOps.pad (letterbox with black fill).
    Saves as optimised PNG.
    """
    try:
        with Image.open(src) as img:
            img = img.convert("RGB")
            img = ImageOps.pad(img, (w, h), color=(0, 0, 0),
                               method=Image.Resampling.LANCZOS)
            img.save(dst, "PNG", optimize=True)
        return True
    except Exception as e:
        print(f"\n  Resize error: {e}")
        return False


# ---------------------------------------------------------------------------
# Progress bar
# ---------------------------------------------------------------------------

def _bar(done: int, total: int, width: int = 20) -> str:
    filled = int(width * done / total)
    return "[" + "#" * filled + "-" * (width - filled) + f"] {done}/{total}"


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main() -> None:
    parser = argparse.ArgumentParser(
        description="Download & resize FIFA WC 2026 team flags"
    )
    parser.add_argument("--team", metavar="CODE",
                        help="Process a single team (e.g. BRA)")
    parser.add_argument("--only-small",  action="store_true",
                        help="Only generate 64×48 flags")
    parser.add_argument("--only-large",  action="store_true",
                        help="Only generate 200×150 flags")
    parser.add_argument("--skip-existing", action="store_true",
                        help="Skip teams whose files already exist")
    parser.add_argument("--prune", action="store_true",
                        help="Delete flag PNGs that are NOT in the qualified team list")
    args = parser.parse_args()

    # Which sizes to generate
    do_small = not args.only_large
    do_large = not args.only_small

    # Which teams to process
    teams = TEAMS
    if args.team:
        code = args.team.upper()
        if code not in TEAMS:
            print(f"ERROR: Unknown team code '{code}'")
            print("Known codes:", ", ".join(sorted(TEAMS)))
            sys.exit(1)
        teams = {code: TEAMS[code]}

    # Ensure output dirs exist
    FLAGS_SM_DIR.mkdir(parents=True, exist_ok=True)
    FLAGS_LG_DIR.mkdir(parents=True, exist_ok=True)

    tmp = FLAGS_SM_DIR / "_tmp_download.png"

    total   = len(teams)
    ok_sm   = 0
    ok_lg   = 0
    failed  = []

    print(f"\nFIFA World Cup 2026 – Flag downloader")
    print(f"Source  : flagcdn.com (160 px → resized)")
    print(f"Small   : {FLAGS_SM_DIR}  ({SM_W}×{SM_H})")
    print(f"Large   : {FLAGS_LG_DIR}  ({LG_W}×{LG_H})")
    print(f"Teams   : {total}")
    print()

    for idx, (code, (name, iso2)) in enumerate(teams.items(), 1):
        dst_sm = FLAGS_SM_DIR / f"{code}.png"
        dst_lg = FLAGS_LG_DIR / f"{code}.png"

        need_sm = do_small and (not args.skip_existing or not dst_sm.exists())
        need_lg = do_large and (not args.skip_existing or not dst_lg.exists())

        label = f"{code:4s}  {name:<28s}"
        print(f"{_bar(idx-1, total)}  {label}", end="\r", flush=True)

        if not need_sm and not need_lg:
            ok_sm += do_small
            ok_lg += do_large
            print(f"{_bar(idx, total)}  {label} SKIP")
            continue

        # Download once
        if not _download(iso2, tmp):
            failed.append(code)
            print(f"{_bar(idx, total)}  {label} FAIL (download)")
            continue

        # Generate requested sizes
        team_ok = True

        if need_sm:
            if _resize(tmp, dst_sm, SM_W, SM_H):
                ok_sm += 1
            else:
                team_ok = False

        if need_lg:
            if _resize(tmp, dst_lg, LG_W, LG_H):
                ok_lg += 1
            else:
                team_ok = False

        if not team_ok:
            failed.append(code)

        sizes_done = []
        if need_sm and dst_sm.exists(): sizes_done.append(f"{SM_W}x{SM_H}")
        if need_lg and dst_lg.exists(): sizes_done.append(f"{LG_W}x{LG_H}")
        status = " + ".join(sizes_done) if sizes_done else "FAIL"
        print(f"{_bar(idx, total)}  {label} {status}")

        # Be polite with the CDN
        time.sleep(0.15)

    # Cleanup temp file
    if tmp.exists():
        tmp.unlink()

    # Prune stale flags (codes no longer in the qualified team list).
    # Only when processing the full list (not a single --team).
    if args.prune and not args.team:
        valid = {f"{c}.png" for c in TEAMS}
        removed = []
        for d in (FLAGS_SM_DIR, FLAGS_LG_DIR):
            for png in d.glob("*.png"):
                if png.name not in valid:
                    png.unlink()
                    removed.append(str(png.relative_to(PROJECT_ROOT)))
        if removed:
            print(f"\nPruned {len(removed)} stale flag(s):")
            for r in sorted(removed):
                print(f"  - {r}")

    # Summary
    print()
    print("=" * 60)
    if do_small:
        print(f"  Small flags ({SM_W}x{SM_H}) : {ok_sm}/{total}")
    if do_large:
        print(f"  Large flags ({LG_W}x{LG_H}) : {ok_lg}/{total}")
    if failed:
        print(f"  Failed               : {', '.join(failed)}")
    else:
        print(f"  All flags downloaded successfully!")
    print()
    print("Next steps:")
    print(f"  1. Upload flags to internal Flash:  pio run --target uploadfs")
    print(f"  2. Flash the firmware:              pio run --target upload")
    print(f"  3. Enjoy!")
    print()


if __name__ == "__main__":
    main()
