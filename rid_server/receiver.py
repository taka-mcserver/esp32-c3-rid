"""
RID Receiver - Decodes Wi-Fi Beacon and BLE Remote ID signals
Supports: ASTM F3411-22a, ASD-STAN prEN 4709-002, GB 42590-2023, GB 46750-2025
"""
import struct
import threading
import time
import queue
from datetime import datetime

CRID_OUI = bytes([0xFA, 0x0B, 0xBC])
CRID_VENDOR_TYPE = 0x0D

MSG_TYPE_NAMES = {
    0: "Basic ID", 1: "Location/Vector", 2: "Authentication",
    3: "Self ID", 4: "System", 5: "Operator ID", 0xF: "Packed Message"
}

ID_TYPE_NAMES = {0: "None", 1: "Serial Number", 2: "CAA Registration ID", 3: "UTM UUID", 4: "Session ID"}
UA_TYPE_NAMES = {0: "None", 1: "Fixed Wing", 2: "Multirotor", 3: "Gyroplane", 4: "Hybrid Lift", 5: "Ornithopter", 6: "Glider", 7: "Kite", 8: "Free Balloon", 9: "Captive Balloon", 10: "Airship", 11: "Parachute", 12: "Rocket", 13: "Tethered Powered", 14: "Ground Obstacle", 15: "Other"}
STATUS_NAMES = {0: "Undeclared", 1: "Ground", 2: "Airborne", 3: "Emergency", 4: "System Failure"}

HORIZ_ACC_NAMES = ["Unknown", "<=1m", "<=2m", "<=3m", "<=4m", "<=6m", "<=10m", "<=15m", "<=20m", "<=25m", "<=30m", "<=35m", "<=40m", "<=45m", "<=50m", "N/A"]
VERT_ACC_NAMES = ["Unknown", "<=1m", "<=2m", "<=3m", "<=4m", "<=5m", "<=6m", "<=7m", "<=8m", "<=9m", "<=10m", "<=15m", "<=20m", "<=25m", "<=30m", "N/A"]
SPEED_ACC_NAMES = ["Unknown", "<=0.1m/s", "<=0.2m/s", "<=0.3m/s", "<=0.4m/s", "<=0.5m/s", "<=0.6m/s", "<=0.7m/s", "<=0.8m/s", "<=0.9m/s", "<=1.0m/s", "<=1.5m/s", "<=2.0m/s", "<=2.5m/s", "<=3.0m/s", "N/A"]

class RIDDecoder:
    """Decodes RID messages from raw bytes"""

    @staticmethod
    def parse_basic_id(data):
        if len(data) < 25:
            return None
        msg_type = (data[0] >> 4) & 0x0F
        if msg_type != 0:
            return None
        version = data[0] & 0x0F
        id_type = (data[1] >> 4) & 0x0F
        ua_type = data[1] & 0x0F
        uas_id = data[2:22].rstrip(b'\x00 \x20').decode('ascii', errors='ignore').strip()
        return {
            'msg_type': 'Basic ID', 'version': version,
            'id_type': ID_TYPE_NAMES.get(id_type, f'Unknown({id_type})'),
            'ua_type': UA_TYPE_NAMES.get(ua_type, f'Unknown({ua_type})'),
            'ua_type_raw': ua_type, 'uas_id': uas_id
        }

    @staticmethod
    def parse_location(data):
        if len(data) < 39:
            return None
        msg_type = (data[0] >> 4) & 0x0F
        if msg_type != 1:
            return None
        version = data[0] & 0x0F
        flags = data[1]
        status = (flags >> 4) & 0x0F
        direction = struct.unpack('<H', data[2:4])[0] / 10.0
        speed_h = data[4] / 10.0
        speed_v = struct.unpack('<b', data[5:6])[0] / 10.0
        lat = struct.unpack('<d', data[6:14])[0]
        lon = struct.unpack('<d', data[14:22])[0]
        alt_baro = struct.unpack('<f', data[22:26])[0] / 100.0
        alt_geo = struct.unpack('<f', data[26:30])[0] / 100.0
        height = struct.unpack('<f', data[30:34])[0] / 100.0
        height_ref = data[34]
        h_acc = data[35] & 0x0F
        v_acc = (data[35] >> 4) & 0x0F
        s_acc = data[36] & 0x0F
        ts_acc = (data[36] >> 4) & 0x0F
        return {
            'msg_type': 'Location', 'version': version,
            'status': STATUS_NAMES.get(status, f'Unknown({status})'),
            'status_raw': status,
            'direction': round(direction, 1), 'speed_h': round(speed_h, 1),
            'speed_v': round(speed_v, 1),
            'latitude': lat, 'longitude': lon,
            'altitude_msl': round(alt_geo, 1), 'altitude_agl': round(height, 1),
            'altitude_baro': round(alt_baro, 1), 'height_ref': height_ref,
            'horiz_accuracy': HORIZ_ACC_NAMES[min(h_acc, 15)],
            'vert_accuracy': VERT_ACC_NAMES[min(v_acc, 15)],
            'speed_accuracy': SPEED_ACC_NAMES[min(s_acc, 15)]
        }

    @staticmethod
    def parse_system(data):
        if len(data) < 26:
            return None
        msg_type = (data[0] >> 4) & 0x0F
        if msg_type != 4:
            return None
        op_loc_type = data[1] & 0x03
        op_lat = struct.unpack('<d', data[2:10])[0]
        op_lon = struct.unpack('<d', data[10:18])[0]
        area_count = struct.unpack('<H', data[18:20])[0]
        area_radius = data[20]
        area_ceiling = struct.unpack('<h', data[21:23])[0] / 10.0
        area_floor = struct.unpack('<h', data[23:25])[0] / 10.0
        classification = data[25] & 0x03
        return {
            'msg_type': 'System',
            'operator_lat': op_lat if op_loc_type > 0 else 0,
            'operator_lon': op_lon if op_loc_type > 0 else 0,
            'area_count': area_count, 'area_radius': area_radius,
            'area_ceiling': area_ceiling, 'area_floor': area_floor,
            'classification': classification
        }

    @staticmethod
    def parse_operator_id(data):
        if len(data) < 23:
            return None
        msg_type = (data[0] >> 4) & 0x0F
        if msg_type != 5:
            return None
        op_id = data[2:22].rstrip(b'\x00 ').decode('ascii', errors='ignore').strip()
        return {'msg_type': 'Operator ID', 'operator_id': op_id}

    @staticmethod
    def parse_self_id(data):
        if len(data) < 24:
            return None
        msg_type = (data[0] >> 4) & 0x0F
        if msg_type != 3:
            return None
        desc_type = data[1]
        desc = data[2:25].rstrip(b'\x00 ').decode('utf-8', errors='ignore').strip()
        return {'msg_type': 'Self ID', 'description': desc}

    @staticmethod
    def decode_message(msg_bytes):
        if not msg_bytes or len(msg_bytes) < 1:
            return None
        msg_type = (msg_bytes[0] >> 4) & 0x0F
        parsers = {0: RIDDecoder.parse_basic_id, 1: RIDDecoder.parse_location,
                   3: RIDDecoder.parse_self_id, 4: RIDDecoder.parse_system,
                   5: RIDDecoder.parse_operator_id}
        parser = parsers.get(msg_type)
        if parser:
            return parser(msg_bytes)
        return {'msg_type': MSG_TYPE_NAMES.get(msg_type, f'Type {msg_type}'), 'raw_len': len(msg_bytes)}


# ── Wi-Fi Beacon Sniffer ──

class WiFiRIDReceiver(threading.Thread):
    def __init__(self, packet_queue, stop_event):
        super().__init__(daemon=True)
        self.queue = packet_queue
        self.stop_event = stop_event
        self.packets_received = 0

    def run(self):
        try:
            from scapy.all import sniff, Dot11, Dot11Elt
            print("[WiFi RX] Starting Wi-Fi Beacon sniffer...")
            sniff(iface=None, prn=self._handle_packet,
                  lfilter=lambda p: p.haslayer(Dot11) and p.type == 0 and p.subtype == 8,
                  store=0, stop_filter=lambda _: self.stop_event.is_set(),
                  timeout=1)
        except Exception as e:
            print(f"[WiFi RX] Sniffer error: {e}")
            print("[WiFi RX] Try running as administrator for monitor mode.")

    def _handle_packet(self, pkt):
        self.packets_received += 1
        try:
            if not pkt.haslayer(Dot11Elt):
                return
            mac = pkt.addr2
            rssi = getattr(pkt, 'dBm_AntSignal', -50)
            elt = pkt[Dot11Elt]
            while elt:
                if elt.ID == 221 and len(elt.info) >= 4:
                    oui = elt.info[:3]
                    if oui == CRID_OUI and elt.info[3] == CRID_VENDOR_TYPE:
                        payload = elt.info[4:]
                        result = RIDDecoder.decode_message(payload)
                        if result:
                            result['mac'] = mac
                            result['rssi'] = rssi
                            result['protocol'] = 'WiFi Beacon'
                            result['timestamp'] = datetime.now().timestamp()
                            self.queue.put(result)
                elt = elt.payload if hasattr(elt, 'payload') else None
        except Exception as e:
            pass


# ── BLE Scanner ──

class BLERIDReceiver(threading.Thread):
    def __init__(self, packet_queue, stop_event):
        super().__init__(daemon=True)
        self.queue = packet_queue
        self.stop_event = stop_event

    def run(self):
        try:
            from bleak import BleakScanner
            import asyncio
            print("[BLE RX] Starting BLE scanner...")

            def detection_callback(device, advertisement_data):
                if self.stop_event.is_set():
                    return
                try:
                    for mfr_id, mfr_data in (advertisement_data.manufacturer_data or {}).items():
                        self._try_decode_ble(mfr_data, device.address, advertisement_data.rssi or -70)
                    for _, svc_data in (advertisement_data.service_data or {}).items():
                        self._try_decode_ble(svc_data, device.address, advertisement_data.rssi or -70)
                except Exception:
                    pass

            async def scan():
                scanner = BleakScanner(detection_callback)
                while not self.stop_event.is_set():
                    await scanner.start()
                    await asyncio.sleep(5)
                    await scanner.stop()
            asyncio.run(scan())
        except ImportError:
            print("[BLE RX] bleak not installed. BLE scanning disabled.")
            print("[BLE RX] Install with: pip install bleak")
        except Exception as e:
            print(f"[BLE RX] Scanner error: {e}")

    def _try_decode_ble(self, data, mac, rssi):
        if not data or len(data) < 4:
            return
        result = RIDDecoder.decode_message(data)
        if result and result.get('uas_id'):
            result['mac'] = mac
            result['rssi'] = rssi
            result['protocol'] = 'BLE'
            result['timestamp'] = datetime.now().timestamp()
            self.queue.put(result)

