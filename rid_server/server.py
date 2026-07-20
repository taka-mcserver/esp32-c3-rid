"""
RID Server - Flask-based web management dashboard
"""
import os
import sys
import json
import time
import queue
import threading
from datetime import datetime

from flask import Flask, render_template, request, jsonify, Response, stream_with_context

# Add current dir to path
sys.path.insert(0, os.path.dirname(__file__))
from database import init_db, get_db, upsert_drone, get_all_drones, get_drone, get_stats, add_history, get_history, get_sim_config, set_sim_config
from receiver import WiFiRIDReceiver, BLERIDReceiver, RIDDecoder
from simulator import RIDSimulator

app = Flask(__name__)

# Global state
packet_queue = queue.Queue()
stop_event = threading.Event()
simulator = None
sse_clients = []
sse_lock = threading.Lock()

@app.route('/')
def index():
    return render_template('index.html')

# ── REST API ──

@app.route('/api/stats')
def api_stats():
    stats = get_stats()
    return jsonify(stats)

@app.route('/api/drones')
def api_drones():
    drones = get_all_drones(online_timeout=60)
    for d in drones:
        d['online'] = bool(d.get('online', 0))
    return jsonify(drones)

@app.route('/api/drones/<uas_id>')
def api_drone_detail(uas_id):
    drone = get_drone(uas_id)
    if not drone:
        return jsonify({'error': 'Not found'}), 404
    history = get_history(uas_id, limit=100)
    drone['history'] = history
    return jsonify(drone)

@app.route('/api/sim_config', methods=['GET', 'POST'])
def api_sim_config():
    if request.method == 'POST':
        data = request.get_json() or {}
        cfg = set_sim_config(data)
        if simulator:
            simulator.set_running(cfg.get('running', False))
        return jsonify(cfg)
    return jsonify(get_sim_config())

@app.route('/api/sim_toggle', methods=['POST'])
def api_sim_toggle():
    data = request.get_json() or {}
    running = data.get('running', False)
    set_sim_config({'running': running})
    if simulator:
        simulator.set_running(running)
    return jsonify({'running': running})

@app.route('/api/status')
def api_status():
    stats = get_stats()
    return jsonify({
        'online': stats['online'],
        'total': stats['total'],
        'sim_running': get_sim_config().get('running', False),
        'packets_received': getattr(receiver_thread, 'packets_received', 0),
        'sim_packets_sent': simulator.packet_count if simulator else 0,
        'timestamp': datetime.now().isoformat()
    })

# ── SSE (Server-Sent Events) ──

@app.route('/api/events')
def sse_events():
    def generate():
        q = queue.Queue()
        with sse_lock:
            sse_clients.append(q)
        try:
            while True:
                try:
                    data = q.get(timeout=5)
                    yield f"data: {json.dumps(data, default=str)}\n\n"
                except queue.Empty:
                    yield f"data: {json.dumps({'type': 'heartbeat', 'ts': time.time()})}\n\n"
        except GeneratorExit:
            with sse_lock:
                if q in sse_clients:
                    sse_clients.remove(q)

    return Response(stream_with_context(generate()),
                    mimetype='text/event-stream',
                    headers={'Cache-Control': 'no-cache',
                             'X-Accel-Buffering': 'no'})

def broadcast_sse(event_type, data):
    msg = {'type': event_type, 'data': data, 'ts': time.time()}
    with sse_lock:
        dead = []
        for q in sse_clients:
            try:
                q.put_nowait(msg)
            except Exception:
                dead.append(q)
        for q in dead:
            sse_clients.remove(q)

# ── Background: Process received packets ──

_current_drone = {}  # Track merged state per drone

def process_packets():
    global _current_drone
    while not stop_event.is_set():
        try:
            pkt = packet_queue.get(timeout=1)
            uas_id = pkt.get('uas_id', '')
            if not uas_id:
                continue

            # Merge with existing drone state
            drone_state = _current_drone.get(uas_id, {})
            drone_state.update({
                'mac': pkt.get('mac', drone_state.get('mac', '')),
                'rssi': pkt.get('rssi', drone_state.get('rssi', 0)),
                'protocol': pkt.get('protocol', drone_state.get('protocol', 'Unknown')),
                'last_seen': pkt.get('timestamp', time.time()),
            })

            if pkt.get('msg_type') == 'Basic ID':
                drone_state['uas_id'] = uas_id
                drone_state['ua_type'] = pkt.get('ua_type_raw', 0)
                drone_state.setdefault('first_seen', pkt.get('timestamp', time.time()))

            elif pkt.get('msg_type') == 'Location':
                drone_state['latitude'] = pkt.get('latitude', 0)
                drone_state['longitude'] = pkt.get('longitude', 0)
                drone_state['altitude_msl'] = pkt.get('altitude_msl', 0)
                drone_state['altitude_agl'] = pkt.get('altitude_agl', 0)
                drone_state['speed_h'] = pkt.get('speed_h', 0)
                drone_state['speed_v'] = pkt.get('speed_v', 0)
                drone_state['heading'] = pkt.get('direction', 0)
                drone_state['status'] = pkt.get('status_raw', 0)

            elif pkt.get('msg_type') == 'System':
                drone_state['operator_lat'] = pkt.get('operator_lat', 0)
                drone_state['operator_lon'] = pkt.get('operator_lon', 0)

            elif pkt.get('msg_type') == 'Operator ID':
                drone_state['operator_id'] = pkt.get('operator_id', '')

            _current_drone[uas_id] = drone_state

            # Persist to DB
            upsert_drone(uas_id, **{k: v for k, v in drone_state.items()
                                     if k in ['first_seen', 'last_seen', 'mac', 'rssi', 'latitude',
                                              'longitude', 'altitude_msl', 'altitude_agl', 'speed_h',
                                              'speed_v', 'heading', 'status', 'ua_type', 'protocol',
                                              'operator_id', 'operator_lat', 'operator_lon']})

            # Add history if location
            if drone_state.get('latitude') and drone_state.get('longitude'):
                add_history(uas_id, drone_state['latitude'], drone_state['longitude'],
                           drone_state.get('altitude_msl', 0), drone_state.get('speed_h', 0),
                           drone_state.get('heading', 0))

            # Broadcast via SSE
            broadcast_sse('drone_update', {
                'uas_id': uas_id,
                'latitude': drone_state.get('latitude'),
                'longitude': drone_state.get('longitude'),
                'altitude_msl': drone_state.get('altitude_msl'),
                'speed_h': drone_state.get('speed_h'),
                'heading': drone_state.get('heading'),
                'rssi': drone_state.get('rssi'),
                'status': drone_state.get('status'),
                'online': True,
                'mac': drone_state.get('mac'),
                'protocol': drone_state.get('protocol'),
            })

        except queue.Empty:
            continue
        except Exception as e:
            pass

# ── Startup ──

receiver_thread = None
processor_thread = None

def start_receivers():
    global receiver_thread, processor_thread, simulator

    # Packet processor
    processor_thread = threading.Thread(target=process_packets, daemon=True)
    processor_thread.start()

    # Wi-Fi receiver
    receiver_thread = WiFiRIDReceiver(packet_queue, stop_event)
    receiver_thread.start()

    # BLE receiver
    ble_thread = BLERIDReceiver(packet_queue, stop_event)
    ble_thread.start()

    # Simulator
    simulator = RIDSimulator(get_sim_config, stop_event)
    simulator.set_running(get_sim_config().get('running', False))
    simulator.start()

    print(f"[Server] Receivers and simulator started")

def main():
    print("=" * 60)
    print("  RID Receiver & Simulator - Web Dashboard")
    print("  Open http://127.0.0.1:5000 in your browser")
    print("=" * 60)

    init_db()
    start_receivers()

    try:
        app.run(host='0.0.0.0', port=5000, debug=False, threaded=True, use_reloader=False)
    except KeyboardInterrupt:
        pass
    finally:
        stop_event.set()
        print("[Server] Shutting down...")

if __name__ == '__main__':
    main()
