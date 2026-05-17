#!/usr/bin/env python3
"""
ingest.py — Ingestion des logs CSV du four verrier dans SQLite.

Usage:
    python ingest.py                   # scanne logs/ pour les nouveaux fichiers
    python ingest.py path/to/file.csv  # ingère un fichier spécifique
    python ingest.py --list            # liste les sessions en base
"""

import re
import sys
import json
import shutil
import sqlite3
import argparse
from datetime import datetime, timezone
from pathlib import Path

import pandas as pd

DB_PATH       = Path(__file__).parent / "db" / "kiln.db"
LOGS_DIR      = Path(__file__).parent / "logs"
PHOTOS_DIR    = Path(__file__).parent / "photos"
PROGRAMS_DIR  = Path(__file__).parent.parent / "data" / "programs"

# ---------------------------------------------------------------------------
# Schéma
# ---------------------------------------------------------------------------
SCHEMA = """
CREATE TABLE IF NOT EXISTS sessions (
    id                 INTEGER PRIMARY KEY AUTOINCREMENT,
    filename           TEXT    UNIQUE NOT NULL,
    log_type           TEXT,       -- 'user' | 'test' | 'maintenance' | 'unknown'
    program_name       TEXT,
    firing_type        TEXT,       -- id depuis firing_types.json (ex: 'bouteilles')
    start_datetime     TEXT,       -- ISO8601
    start_temp         REAL,
    firmware_version   TEXT,
    kp                 REAL,
    ki                 REAL,
    kd                 REAL,
    ema_alpha          REAL,
    boost_enabled      INTEGER,
    pid_reset_disabled INTEGER,
    log_interval_ms    INTEGER,
    adaptive_logging   INTEGER,
    num_steps          INTEGER,
    steps_json         TEXT,       -- JSON : [{"target":750,"rate":5.0,"hold":1800},...]
    notes              TEXT,       -- annotation manuelle
    imported_at        TEXT        -- ISO8601
);

CREATE TABLE IF NOT EXISTS photos (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    session_id  INTEGER NOT NULL REFERENCES sessions(id) ON DELETE CASCADE,
    filename    TEXT    NOT NULL,
    caption     TEXT,
    added_at    TEXT                -- ISO8601
);

CREATE INDEX IF NOT EXISTS idx_photos_session ON photos(session_id);

CREATE TABLE IF NOT EXISTS datapoints (
    id                   INTEGER PRIMARY KEY AUTOINCREMENT,
    session_id           INTEGER REFERENCES sessions(id) ON DELETE CASCADE,
    timestamp            TEXT,
    elapsed_sec          REAL,
    current_temp         REAL,
    target_temp          REAL,
    phase                TEXT,
    step_num             INTEGER,
    -- colonnes test + maintenance
    raw_temp             REAL,
    pid_output           REAL,
    ssr2_pwm             INTEGER,
    -- colonnes maintenance uniquement
    filtered_temp        REAL,
    error                REAL,
    p_term               REAL,
    i_term               REAL,
    d_term               REAL,
    kp_rt                REAL,
    ki_rt                REAL,
    kd_rt                REAL,
    ramp_rate            REAL,
    hold_time_sec        INTEGER,
    effective_hold_sec   INTEGER,
    stabilizing_time_sec INTEGER,
    initial_boost        INTEGER,
    log_reason           TEXT
);

CREATE INDEX IF NOT EXISTS idx_dp_session ON datapoints(session_id);
CREATE INDEX IF NOT EXISTS idx_dp_elapsed ON datapoints(session_id, elapsed_sec);
"""

# ---------------------------------------------------------------------------
# Migration (ajout de colonnes sur base existante)
# ---------------------------------------------------------------------------

def migrate(conn: sqlite3.Connection):
    existing = {row[1] for row in conn.execute("PRAGMA table_info(sessions)")}
    if 'firing_type' not in existing:
        conn.execute("ALTER TABLE sessions ADD COLUMN firing_type TEXT")
        conn.commit()

    # Table photos (créée par SCHEMA IF NOT EXISTS, mais au cas où base très ancienne)
    conn.executescript("""
        CREATE TABLE IF NOT EXISTS photos (
            id          INTEGER PRIMARY KEY AUTOINCREMENT,
            session_id  INTEGER NOT NULL REFERENCES sessions(id) ON DELETE CASCADE,
            filename    TEXT    NOT NULL,
            caption     TEXT,
            added_at    TEXT
        );
        CREATE INDEX IF NOT EXISTS idx_photos_session ON photos(session_id);
    """)


# ---------------------------------------------------------------------------
# Parseurs de métadonnées
# ---------------------------------------------------------------------------

def _parse_kv(line: str) -> dict:
    """Extrait les paires KEY:VALUE d'une ligne (hors bloc [...])."""
    clean = re.sub(r'\s*\[.*\]', '', line)
    return {m.group(1).upper(): m.group(2) for m in re.finditer(r'(\w+):(\S+)', clean)}


def _parse_steps(steps_raw: str) -> list:
    """Convertit '750@5.0/1800,300@0/600' en liste de dicts."""
    steps = []
    for token in steps_raw.split(','):
        m = re.match(r'([\d.]+)@([\d.]+)/([\d]+)', token.strip())
        if m:
            steps.append({
                "target": float(m.group(1)),
                "rate":   float(m.group(2)),
                "hold":   int(m.group(3)),
            })
    return steps


def parse_metadata(csv_path: Path) -> dict:
    """Lit les lignes # en début de fichier et retourne un dict de métadonnées."""
    meta: dict = {}
    with open(csv_path, encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if not line.startswith('#'):
                break
            content = line[1:].strip()
            meta.update(_parse_kv(content))
            bracket = re.search(r'\[([^\]]+)\]', content)
            if bracket:
                meta['_STEPS_RAW'] = bracket.group(1)
    return meta


def detect_log_type(filename: str) -> str:
    name = Path(filename).name.lower()
    if name.startswith("maint_"):  return "maintenance"
    if name.startswith("test_"):   return "test"
    if name.startswith("user_"):   return "user"
    return "unknown"


def read_program_type(program_name: str) -> str | None:
    """Tente de lire le champ 'type' dans data/programs/<program_name>.json."""
    if not program_name:
        return None
    for candidate in [program_name, program_name.lower()]:
        p = PROGRAMS_DIR / f"{candidate}.json"
        if p.exists():
            try:
                return json.loads(p.read_text(encoding="utf-8")).get("type")
            except Exception:
                return None
    return None


# ---------------------------------------------------------------------------
# Ingestion d'un fichier
# ---------------------------------------------------------------------------

def ingest_file(csv_path: Path, conn: sqlite3.Connection) -> bool:
    """Insère un CSV dans la base. Retourne True si inséré, False si déjà présent."""
    filename = csv_path.name

    if conn.execute("SELECT id FROM sessions WHERE filename=?", (filename,)).fetchone():
        print(f"  [skip]  {filename}  (déjà en base)")
        return False

    meta     = parse_metadata(csv_path)
    log_type = detect_log_type(filename)
    steps    = _parse_steps(meta.get('_STEPS_RAW', ''))

    def flt(key): return float(meta[key]) if key in meta else None
    def iit(key): return int(meta[key])   if key in meta else None

    program_name = meta.get('PROGRAM')
    firing_type  = read_program_type(program_name)

    now_iso = datetime.now(timezone.utc).isoformat()
    cur = conn.execute("""
        INSERT INTO sessions
            (filename, log_type, program_name, firing_type, start_datetime, start_temp,
             firmware_version, kp, ki, kd, ema_alpha,
             boost_enabled, pid_reset_disabled,
             log_interval_ms, adaptive_logging,
             num_steps, steps_json, imported_at)
        VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)
    """, (
        filename, log_type,
        program_name, firing_type,
        meta.get('START'),
        flt('STARTTEMP'), meta.get('FIRMWARE'),
        flt('KP'), flt('KI'), flt('KD'), flt('EMA'),
        iit('BOOST'), iit('PIDRESET'),
        iit('INTERVAL'), iit('ADAPTIVE'),
        iit('STEPS'),
        json.dumps(steps) if steps else None,
        now_iso,
    ))
    session_id = cur.lastrowid

    df = pd.read_csv(csv_path, comment='#')
    df.columns = [c.strip() for c in df.columns]

    def col(row, name):
        return row[name] if name in df.columns else None

    rows = []
    for _, row in df.iterrows():
        r: dict = {
            'session_id':   session_id,
            'timestamp':    col(row, 'Timestamp'),
            'elapsed_sec':  col(row, 'ElapsedSeconds'),
            'current_temp': col(row, 'CurrentTemp'),
            'target_temp':  col(row, 'TargetTemp'),
            'phase':        col(row, 'Phase'),
            'step_num':     col(row, 'Step'),
        }
        if log_type in ('test', 'maintenance'):
            r['raw_temp']   = col(row, 'RawTemp')
            r['pid_output'] = col(row, 'PidOutput')
            r['ssr2_pwm']   = col(row, 'SSR2_PWM')
        if log_type == 'maintenance':
            r['filtered_temp']        = col(row, 'FilteredTemp')
            r['error']                = col(row, 'Error')
            r['p_term']               = col(row, 'Pterm')
            r['i_term']               = col(row, 'Iterm')
            r['d_term']               = col(row, 'Dterm')
            r['kp_rt']                = col(row, 'PidKp')
            r['ki_rt']                = col(row, 'PidKi')
            r['kd_rt']                = col(row, 'PidKd')
            r['ramp_rate']            = col(row, 'RampRate')
            r['hold_time_sec']        = col(row, 'HoldTime')
            r['effective_hold_sec']   = col(row, 'EffectiveHoldSec')
            r['stabilizing_time_sec'] = col(row, 'StabilizingTimeSec')
            r['initial_boost']        = col(row, 'InitialBoost')
            r['log_reason']           = col(row, 'LogReason')
        rows.append(r)

    if rows:
        cols = list(rows[0].keys())
        ph   = ','.join(['?'] * len(cols))
        conn.executemany(
            f"INSERT INTO datapoints ({','.join(cols)}) VALUES ({ph})",
            [[r.get(c) for c in cols] for r in rows],
        )

    conn.commit()
    print(f"  [ok]    {filename}  —  {len(rows)} points  (session {session_id})")
    return True


# ---------------------------------------------------------------------------
# Commande --list
# ---------------------------------------------------------------------------

def list_sessions(conn: sqlite3.Connection):
    rows = conn.execute("""
        SELECT s.id, s.log_type, s.program_name, s.start_datetime,
               s.firmware_version, s.kp, s.ki, s.kd,
               COUNT(d.id) AS points
        FROM sessions s
        LEFT JOIN datapoints d ON d.session_id = s.id
        GROUP BY s.id
        ORDER BY s.start_datetime
    """).fetchall()

    if not rows:
        print("Base vide.")
        return

    print(f"\n{'ID':>3}  {'Type':<12} {'Programme':<22} {'Début':<20} "
          f"{'Firmware':<10} {'Kp':>5} {'Ki':>6} {'Kd':>5}  {'Points':>6}")
    print("-" * 100)
    for r in rows:
        sid, ltype, prog, start, fw, kp, ki, kd, pts = r
        print(f"{sid:>3}  {(ltype or '?'):<12} {(prog or '?'):<22} "
              f"{(start or '?'):<20} {(fw or '?'):<10} "
              f"{kp or 0:>5.1f} {ki or 0:>6.3f} {kd or 0:>5.1f}  {pts:>6}")


# ---------------------------------------------------------------------------
# Commande --photos-for
# ---------------------------------------------------------------------------

def add_photos(session_id: int, photo_paths: list[Path], caption: str | None,
               conn: sqlite3.Connection):
    row = conn.execute("SELECT id FROM sessions WHERE id=?", (session_id,)).fetchone()
    if not row:
        print(f"Session {session_id} introuvable en base.")
        return

    dest_dir = PHOTOS_DIR / f"session_{session_id}"
    dest_dir.mkdir(parents=True, exist_ok=True)

    now_iso = datetime.now(timezone.utc).isoformat()
    for src in photo_paths:
        dest = dest_dir / src.name
        shutil.copy2(src, dest)
        conn.execute(
            "INSERT INTO photos (session_id, filename, caption, added_at) VALUES (?,?,?,?)",
            (session_id, src.name, caption, now_iso),
        )
        print(f"  [photo] {src.name} → {dest}")
    conn.commit()
    print(f"{len(photo_paths)} photo(s) associée(s) à la session {session_id}.")


def main():
    parser = argparse.ArgumentParser(
        description="Ingestion CSV four verrier → SQLite",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""Exemples :
  python ingest.py                            # scanne logs/ pour les nouveaux fichiers
  python ingest.py path/to/file.csv           # ingère un fichier spécifique
  python ingest.py --list                     # liste les sessions en base
  python ingest.py --photos-for 3 a.jpg b.jpg --caption "Résultat final"
""",
    )
    parser.add_argument("files", nargs="*", help="Fichiers CSV (défaut : scan logs/)")
    parser.add_argument("--list", "-l", action="store_true", help="Liste les sessions en base")
    parser.add_argument("--photos-for", metavar="SESSION_ID", type=int,
                        help="Associe des photos à une session (id entier)")
    parser.add_argument("--caption", metavar="TEXTE", help="Légende commune pour les photos")
    args = parser.parse_args()

    DB_PATH.parent.mkdir(exist_ok=True)
    conn = sqlite3.connect(DB_PATH)
    conn.execute("PRAGMA foreign_keys = ON")
    conn.executescript(SCHEMA)
    migrate(conn)

    if args.list:
        list_sessions(conn)
        conn.close()
        return

    if args.photos_for is not None:
        if not args.files:
            print("Précisez au moins un fichier photo après --photos-for SESSION_ID.")
            conn.close()
            return
        add_photos(args.photos_for, [Path(f) for f in args.files], args.caption, conn)
        conn.close()
        return

    if args.files:
        paths = [Path(f) for f in args.files]
    else:
        LOGS_DIR.mkdir(exist_ok=True)
        paths = sorted(LOGS_DIR.glob("*.csv"))
        if not paths:
            print(f"Aucun CSV trouvé dans {LOGS_DIR}/")
            print("Copiez vos fichiers CSV dans ce dossier, ou passez un chemin en argument.")
            conn.close()
            return

    print(f"Base   : {DB_PATH}")
    print(f"Fichiers : {len(paths)}")
    inserted = sum(ingest_file(p, conn) for p in paths)
    print(f"\n{inserted}/{len(paths)} fichier(s) ingéré(s).")
    conn.close()


if __name__ == "__main__":
    main()
