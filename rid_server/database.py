import sqlite3
import json
import threading
import os
from datetime import datetime, timedelta

DB_PATH = os.path.join(os.path.dirname(__file__), "rid_data.db")

_local = threading.local()

def get_db():
    if not hasattr(_local, "conn") or _local.conn is None:
        _local.conn = sqlite3.connect(DB_PATH)
        _local.conn.row_factory = sqlite3.Row
        _local.conn.execute("PRAGMA journal_mode=WAL")
        _local.conn.execute("PRAGMA busy_timeout=5000")
    return _local.conn

def init_db():
    db = get_db()
    db.executescript("""
        CREATE TABLE IF NOT EXISTS drones (
            uas_id TEXT PRIMARY KEY,
            drone_name TEXT DEFAULT "",
            mac TEXT DEFAULT "",
            first_seen REAL NOT NULL,
            last_seen REAL NOT NULL,
            rssi INTEGER DEFAULT 0,
            latitude REAL DEFAULT 0,
            longitude REAL DEFAULT 0,
            altitude_msl REAL DEFAULT 0,
            altitude_agl REAL DEFAULT 0,
            speed_h REAL DEFAULT 0,
            speed_v REAL DEFAULT 0,
            heading REAL DEFAULT 0,
            status INTEGER DEFAULT 0,
            ua_type INTEGER DEFAULT 0,
            protocol TEXT DEFAULT "Unknown",
            operator_id TEXT DEFAULT "",
            operator_lat REAL DEFAULT 0,
            operator_lon REAL DEFAULT 0,
            raw_data TEXT DEFAULT "{}"
        );
        CREATE TABLE IF NOT EXISTS sim_config (
            key TEXT PRIMARY KEY,
            value TEXT NOT NULL
        );
        CREATE TABLE IF NOT EXISTS drone_history (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            uas_id TEXT NOT NULL,
            latitude REAL,
            longitude REAL,
            altitude_msl REAL,
            speed_h REAL,
            heading REAL,
            recorded_at REAL NOT NULL,
            FOREIGN KEY (uas_id) REFERENCES drones(uas_id)
        );
        CREATE INDEX IF NOT EXISTS idx_history_uas ON drone_history(uas_id, recorded_at);
    """)
    db.commit()

def upsert_drone(uas_id, **fields):
    db = get_db()
    now = datetime.now().timestamp()
    row = db.execute("SELECT uas_id FROM drones WHERE uas_id=?", (uas_id,)).fetchone()
    if row:
        cols = ", ".join(f"{k}=?" for k in fields.keys())
        vals = list(fields.values())
        db.execute(f"UPDATE drones SET {cols}, last_seen=? WHERE uas_id=?", vals + [now, uas_id])
    else:
        fields["uas_id"] = uas_id
        fields.setdefault("first_seen", now)
        fields.setdefault("last_seen", now)
        keys = list(fields.keys())
        placeholders = ", ".join("?" * len(keys))
        cols = ", ".join(keys)
        vals = [fields[k] for k in keys]
        db.execute(f"INSERT INTO drones ({cols}) VALUES ({placeholders})", vals)
    db.commit()
    return True

def get_all_drones(online_timeout=60):
    db = get_db()
    cutoff = datetime.now().timestamp() - online_timeout
    rows = db.execute(
        "SELECT *, (CASE WHEN last_seen >= ? THEN 1 ELSE 0 END) AS online FROM drones ORDER BY last_seen DESC",
        (cutoff,)
    ).fetchall()
    return [dict(r) for r in rows]

def get_drone(uas_id):
    db = get_db()
    row = db.execute("SELECT * FROM drones WHERE uas_id=?", (uas_id,)).fetchone()
    return dict(row) if row else None

def get_stats():
    db = get_db()
    cutoff = datetime.now().timestamp() - 60
    online = db.execute("SELECT COUNT(*) FROM drones WHERE last_seen >= ?", (cutoff,)).fetchone()[0]
    total = db.execute("SELECT COUNT(*) FROM drones").fetchone()[0]
    return {"online": online, "total": total}

def add_history(uas_id, lat, lon, alt_msl, speed_h, heading):
    db = get_db()
    db.execute(
        "INSERT INTO drone_history (uas_id, latitude, longitude, altitude_msl, speed_h, heading, recorded_at) VALUES (?,?,?,?,?,?,?)",
        (uas_id, lat, lon, alt_msl, speed_h, heading, datetime.now().timestamp())
    )
    db.execute("DELETE FROM drone_history WHERE id NOT IN (SELECT id FROM drone_history WHERE uas_id=? ORDER BY recorded_at DESC LIMIT 200)", (uas_id,))
    db.commit()

def get_history(uas_id, limit=100):
    db = get_db()
    rows = db.execute(
        "SELECT * FROM drone_history WHERE uas_id=? ORDER BY recorded_at DESC LIMIT ?",
        (uas_id, limit)
    ).fetchall()
    return [dict(r) for r in rows]

_DEFAULT_SIM_CONFIG = {
    "running": False,
    "uas_id": "SIM-DRONE-001",
    "drone_name": "Simulated Drone",
    "ua_type": 2,
    "latitude": 39.9042,
    "longitude": 116.4074,
    "altitude_msl": 100.0,
    "altitude_agl": 80.0,
    "speed_horizontal": 5.0,
    "speed_vertical": 0.0,
    "heading": 90.0,
    "status": 2,
    "operator_id": "OP-SIM-001",
    "operator_lat": 39.9045,
    "operator_lon": 116.4070,
}

def get_sim_config():
    db = get_db()
    rows = db.execute("SELECT key, value FROM sim_config").fetchall()
    config = dict(_DEFAULT_SIM_CONFIG)
    for r in rows:
        key, val = r["key"], r["value"]
        if key == "running":
            config[key] = val == "true"
        elif key in ("latitude", "longitude", "altitude_msl", "altitude_agl", "speed_horizontal", "speed_vertical", "heading", "operator_lat", "operator_lon"):
            config[key] = float(val)
        elif key in ("ua_type", "status"):
            config[key] = int(val)
        else:
            config[key] = val
    return config

def set_sim_config(data):
    db = get_db()
    for key, val in data.items():
        db.execute(
            "INSERT OR REPLACE INTO sim_config (key, value) VALUES (?, ?)",
            (key, str(val).lower() if isinstance(val, bool) else str(val))
        )
    db.commit()
    return get_sim_config()
