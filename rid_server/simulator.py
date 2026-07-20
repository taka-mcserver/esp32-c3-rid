"""
RID Simulator - Transmits Wi-Fi Beacon frames with RID data
"""
import struct
import threading
import time
import random
import os
import sys

CRID_OUI = bytes([0xFA, 0x0B, 0xBC])
CRID_VENDOR_TYPE = 0x0D

def build_basic_id_message(uas_id, ua_type=2):
    """Build GB42590/ASD-STAN Basic ID message (25 bytes)"""
    msg = bytearray(25)
    msg[0] = 0x01  # msg_type=0 (Basic ID), version=1
    msg[1] = (2 << 4) | (ua_type & 0x0F)  # id_type=2 (CAA Registration), ua_type
    uas_bytes = uas_id.encode('ascii', errors='ignore')[:20].ljust(20, b' ')
    msg[2:22] = uas_bytes
    return bytes(msg)

def build_location_message(lat, lon, alt_msl, alt_agl, speed_h, speed_v, heading, status=2):
    """Build GB42590/ASD-STAN Location message (39 bytes)"""
    msg = bytearray(39)
    msg[0] = 0x11  # msg_type=1 (Location), version=1
    msg[1] = (status << 4) | 0x0F  # status + flags
    struct.pack_into('<H', msg, 2, int(heading * 10))
    msg[4] = min(int(speed_h * 10), 255)
    struct.pack_into('<b', msg, 5, max(-128, min(127, int(speed_v * 10))))
    struct.pack_into('<d', msg, 6, lat)
    struct.pack_into('<d', msg, 14, lon)
    struct.pack_into('<f', msg, 22, alt_msl * 100)  # alt baro in cm
    struct.pack_into('<f', msg, 26, alt_msl * 100)  # alt geo in cm
    struct.pack_into('<f', msg, 30, alt_agl * 100)  # height in cm
    msg[34] = 0  # height_ref = over takeoff
    msg[35] = 0x00  # accuracy
    msg[36] = 0x00
    return bytes(msg)

def build_system_message(op_lat, op_lon, area_count=1, area_radius=100):
    """Build System message with operator location"""
    msg = bytearray(28)
    msg[0] = 0x41  # msg_type=4, version=1
    msg[1] = 0x01  # operator_location_type = takeoff
    struct.pack_into('<d', msg, 2, op_lat)
    struct.pack_into('<d', msg, 10, op_lon)
    struct.pack_into('<H', msg, 18, area_count)
    msg[20] = area_radius
    struct.pack_into('<h', msg, 21, 400)  # ceiling 40m
    struct.pack_into('<h', msg, 23, 0)    # floor 0m
    msg[25] = 0  # classification
    return bytes(msg)

def build_operator_id_message(operator_id):
    """Build Operator ID message"""
    msg = bytearray(23)
    msg[0] = 0x51  # msg_type=5, version=1
    op_bytes = operator_id.encode('ascii', errors='ignore')[:20].ljust(20, b' ')
    msg[2:22] = op_bytes
    return bytes(msg)

def build_vendor_ie(payload):
    """Build vendor-specific IE for Wi-Fi Beacon"""
    ie = bytearray(4 + len(payload))
    ie[0] = 221  # Vendor Specific IE
    ie[1] = 3 + len(payload)  # length
    ie[2:5] = CRID_OUI
    ie[4] = CRID_VENDOR_TYPE
    ie[5:] = payload
    return bytes(ie)


class RIDSimulator(threading.Thread):
    def __init__(self, get_config_callback, stop_event):
        super().__init__(daemon=True)
        self.get_config = get_config_callback
        self.stop_event = stop_event
        self.running = False
        self.packet_count = 0

    def set_running(self, running):
        self.running = running
        if not running:
            self.packet_count = 0

    def run(self):
        print("[Sim TX] Simulator thread started")
        while not self.stop_event.is_set():
            cfg = self.get_config()
            if cfg.get('running', False) and self.running:
                self._send_beacons(cfg)
            time.sleep(1)

    def _send_beacons(self, cfg):
        try:
            from scapy.all import RadioTap, Dot11, Dot11Beacon, Dot11Elt, sendp

            uas_id = cfg.get('uas_id', 'SIM-DRONE-001')
            lat = cfg.get('latitude', 39.9042)
            lon = cfg.get('longitude', 116.4074)
            alt_msl = cfg.get('altitude_msl', 100)
            alt_agl = cfg.get('altitude_agl', 80)
            speed_h = cfg.get('speed_horizontal', 5)
            speed_v = cfg.get('speed_vertical', 0)
            heading = cfg.get('heading', 90)
            status = cfg.get('status', 2)
            ua_type = cfg.get('ua_type', 2)
            op_id = cfg.get('operator_id', 'OP-SIM-001')
            op_lat = cfg.get('operator_lat', 39.9045)
            op_lon = cfg.get('operator_lon', 116.4070)

            # Randomize MAC for simulation
            mac = ':'.join(f'{random.randint(0,255):02x}' for _ in range(6))
            bssid = mac
            ssid = f'RID-{uas_id[:8]}'

            # Build RID messages
            basic = build_basic_id_message(uas_id, ua_type)
            loc = build_location_message(lat, lon, alt_msl, alt_agl, speed_h, speed_v, heading, status)
            sysmsg = build_system_message(op_lat, op_lon)
            opmsg = build_operator_id_message(op_id)

            # Pack multiple messages into beacon
            for payload in [basic, loc, sysmsg, opmsg]:
                ie = build_vendor_ie(payload)

                pkt = (RadioTap() /
                       Dot11(type=0, subtype=8, addr1='ff:ff:ff:ff:ff:ff',
                             addr2=mac, addr3=bssid) /
                       Dot11Beacon(cap='ESS') /
                       Dot11Elt(ID='SSID', info=ssid.encode()) /
                       Dot11Elt(ID='Rates', info=b'\x82\x84\x8b\x96') /
                       Dot11Elt(ID=221, info=ie[2:]))

                try:
                    sendp(pkt, iface=None, verbose=0, count=1, inter=0.1)
                    self.packet_count += 1
                except Exception as e:
                    print(f"[Sim TX] Send error: {e}")
                    return

                if self.stop_event.is_set():
                    return
                time.sleep(0.15)

        except ImportError:
            print("[Sim TX] scapy not available")
        except Exception as e:
            print(f"[Sim TX] Beacon error: {e}")


def build_location_message_packed(lat, lon, alt_msl, alt_agl, speed_h, speed_v, heading, status=2):
    return build_location_message(lat, lon, alt_msl, alt_agl, speed_h, speed_v, heading, status)

def build_basic_id_message_packed(uas_id, ua_type=2):
    return build_basic_id_message(uas_id, ua_type)

