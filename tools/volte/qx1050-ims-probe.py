#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""VoLTE step 1 probe: publish QMI service 770 ("IMS data", v1) on QRTR, as 81voltd does, and
log what the modem's IMS stack asks for. Brings up NOTHING: Start Connection (0x20) is answered
with an error, as 81voltd answers when its bearer fails, so oFono and the data context stay
untouched. Message layout from 81voltd's imsd.qmi (Richard Acayan, GPL-2.0-or-later).

usage: qx1050-ims-probe.py [seconds, default 600]
Output: one line per event, flushed at once (stream it to the host).
Never prints IMSI/IMEI (the service carries none); APNs and addresses are printed.
"""
import ctypes, os, select, socket, struct, sys, time

AF_QIPCRTR = 42
CTRL_PORT = 0xfffffffe
NEW_SERVER, DEL_SERVER = 4, 5
SERVICE, VERSION, INSTANCE = 770, 1, 0
CTRL_NAMES = {1: "DATA", 2: "HELLO", 3: "BYE", 4: "NEW_SERVER", 5: "DEL_SERVER",
              6: "DEL_CLIENT", 7: "RESUME_TX", 8: "EXIT", 9: "PING", 10: "NEW_LOOKUP", 11: "DEL_LOOKUP"}
IPFAM = {0: "IPv4", 1: "IPv6"}   # 81voltd qvd-server.c: IMSD_AF_IPV4 = 0, IMSD_AF_IPV6 = 1

T0 = time.monotonic()
def log(msg):
    print("[%8.2f] %s" % (time.monotonic() - T0, msg), flush=True)

libc = ctypes.CDLL(None, use_errno=True)
class SA(ctypes.Structure):
    _fields_ = [("family", ctypes.c_ushort), ("node", ctypes.c_uint), ("port", ctypes.c_uint)]

fd = libc.socket(AF_QIPCRTR, socket.SOCK_DGRAM, 0)
if fd < 0:
    sys.exit("socket(AF_QIPCRTR): errno %d" % ctypes.get_errno())
me = SA(); ln = ctypes.c_uint(ctypes.sizeof(me))
libc.getsockname(fd, ctypes.byref(me), ctypes.byref(ln))

def sendto(node, port, pkt):
    d = SA(AF_QIPCRTR, node, port); b = ctypes.create_string_buffer(pkt, len(pkt))
    if libc.sendto(fd, b, len(pkt), 0, ctypes.byref(d), ctypes.sizeof(d)) < 0:
        raise OSError(ctypes.get_errno(), "sendto")

def recvfrom():
    buf = ctypes.create_string_buffer(65536); src = SA(); sl = ctypes.c_uint(ctypes.sizeof(src))
    n = libc.recvfrom(fd, buf, 65536, 0, ctypes.byref(src), ctypes.byref(sl))
    if n < 0:
        raise OSError(ctypes.get_errno(), "recvfrom")
    return buf.raw[:n], src.node, src.port

def ctrl(cmd):
    sendto(me.node, CTRL_PORT, struct.pack("<5I", cmd, SERVICE, INSTANCE << 8 | VERSION, me.node, me.port))

tlv = lambda t, d: struct.pack("<BH", t, len(d)) + d
def tlvs(body):
    out, i = {}, 0
    while i + 3 <= len(body):
        t, l = struct.unpack_from("<BH", body, i); out[t] = body[i + 3:i + 3 + l]; i += 3 + l
    return out

def decode_spec(v):
    """wds_spec: string apn, u32 unk, u32 profiles_select, u32 ip_family, u16 idx3gpp, u16 idx3gpp2.
    The string's length prefix is u16 or u8 depending on the encoder; pick the one that fits."""
    for lsz, fmt in ((2, "<H"), (1, "<B")):
        if len(v) >= lsz:
            n = struct.unpack_from(fmt, v)[0]
            if lsz + n + 16 == len(v):
                apn = v[lsz:lsz + n].decode("ascii", "replace")
                unk, sel, fam, i3, i2 = struct.unpack_from("<IIIHH", v, lsz + n)
                return "apn=%r ip_family=%s profiles_select=%d profile_3gpp=%d profile_3gpp2=%d unk=%d (len prefix u%d)" % (
                    apn, IPFAM.get(fam, fam), sel, i3, i2, unk, lsz * 8)
    return "undecoded wds_spec %s" % v.hex()

def respond(node, port, txn, mid, body):
    sendto(node, port, struct.pack("<BHHH", 2, txn, mid, len(body)) + body)

def main():
    dur = float(sys.argv[1]) if len(sys.argv) > 1 else 600
    ctrl(NEW_SERVER)
    log("published service %d v%d on node %d port %d; listening %d s" % (SERVICE, VERSION, me.node, me.port, dur))
    end = time.monotonic() + dur
    try:
        while time.monotonic() < end:
            if not select.select([fd], [], [], 1)[0]:
                continue
            pkt, node, port = recvfrom()
            if port == CTRL_PORT:
                cmd = struct.unpack_from("<I", pkt)[0] if len(pkt) >= 4 else -1
                if cmd in (3, 6):   # BYE, DEL_CLIENT: a client went away
                    log("ctrl %s from node %d: %s" % (CTRL_NAMES.get(cmd, cmd), node, pkt[4:].hex()))
                continue
            if len(pkt) < 7:
                log("short packet from %d:%d %s" % (node, port, pkt.hex())); continue
            typ, txn, mid, l = struct.unpack_from("<BHHH", pkt)
            t = tlvs(pkt[7:7 + l])
            desc = " ".join("%02x:%s" % (k, v.hex()) for k, v in sorted(t.items()))
            if typ != 0:
                log("type %d msg 0x%02x from %d:%d tlvs %s" % (typ, mid, node, port, desc)); continue
            if mid == 0x20:
                conn = struct.unpack("<I", t[0x10])[0] if len(t.get(0x10, b"")) == 4 else None
                sub = struct.unpack("<I", t[0x13])[0] if len(t.get(0x13, b"")) == 4 else None
                log("REQ 0x20 START CONNECTION from %d:%d txn %d conn %s sub %s %s" % (
                    node, port, txn, conn, sub, decode_spec(t[1]) if 1 in t else "no spec TLV"))
                respond(node, port, txn, mid, tlv(2, struct.pack("<HH", 1, 1)))
                log("  answered: error (probe brings up no bearer)")
            elif mid == 0x21:
                log("REQ 0x21 STOP CONNECTION from %d:%d txn %d tlvs %s" % (node, port, txn, desc))
                respond(node, port, txn, mid, tlv(2, struct.pack("<HH", 0, 0)) + tlv(0x10, b"\xfa") +
                        tlv(0x11, t.get(0x10, b"\0\0\0\0")[:4].ljust(4, b"\0")))
            else:
                log("REQ 0x%02x (no-op) from %d:%d txn %d tlvs %s" % (mid, node, port, txn, desc))
                respond(node, port, txn, mid, tlv(2, struct.pack("<HH", 0, 0)))
    except KeyboardInterrupt:
        pass
    finally:
        ctrl(DEL_SERVER)
        log("service withdrawn, end")

main()
