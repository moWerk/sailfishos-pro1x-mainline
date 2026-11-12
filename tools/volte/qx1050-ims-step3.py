#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""VoLTE step 3: a minimal stand-in for 81voltd, in one process, without ModemManager or oFono.
- publishes QMI service 770 ("IMS data", v1) on QRTR; message layout from 81voltd's imsd.qmi;
- sets the voice domain to ps-preferred (non-permanent) so the modem asks for the IMS PDN;
- on Start Connection (0x20): a WDS client bound to mux MUX starts the PDN with the requested
  3GPP profile index and IP family, answers success and reports the address (indication 0x20);
- on Stop Connection (0x21) or at the end: stops the PDN; at the end cs-preferred is set back;
- every 10 s logs the IMSA registration status (read-only qmicli).
usage: qx1050-ims-step3.py [seconds, default 900] [mux, default 1]
The PDN address is logged shortened (prefix only): it stays out of the log as a whole.
"""
import ctypes, os, select, socket, struct, subprocess, sys, time

DUR = float(sys.argv[1]) if len(sys.argv) > 1 else 900
MUX = int(sys.argv[2]) if len(sys.argv) > 2 else 1
T0 = time.monotonic()
def log(m): print("[%8.2f] %s" % (time.monotonic() - T0, m), flush=True)

libc = ctypes.CDLL(None, use_errno=True)
class SA(ctypes.Structure):
    _fields_ = [("family", ctypes.c_ushort), ("node", ctypes.c_uint), ("port", ctypes.c_uint)]
def qsock():
    fd = libc.socket(42, socket.SOCK_DGRAM, 0)
    me = SA(); ln = ctypes.c_uint(ctypes.sizeof(me)); libc.getsockname(fd, ctypes.byref(me), ctypes.byref(ln))
    return fd, me
def sendto(fd, node, port, pkt):
    d = SA(42, node, port); b = ctypes.create_string_buffer(pkt, len(pkt))
    if libc.sendto(fd, b, len(pkt), 0, ctypes.byref(d), ctypes.sizeof(d)) < 0:
        raise OSError(ctypes.get_errno(), "sendto")
def recvfrom(fd):
    buf = ctypes.create_string_buffer(65536); src = SA(); sl = ctypes.c_uint(ctypes.sizeof(src))
    n = libc.recvfrom(fd, buf, 65536, 0, ctypes.byref(src), ctypes.byref(sl))
    return buf.raw[:max(n, 0)], src.node, src.port
tlv = lambda t, d: struct.pack("<BH", t, len(d)) + d
def tlvs(body):
    out, i = {}, 0
    while i + 3 <= len(body):
        t, l = struct.unpack_from("<BH", body, i); out[t] = body[i + 3:i + 3 + l]; i += 3 + l
    return out
def qstr(s): b = s.encode(); return struct.pack("<B", len(b)) + b   # u8 length, as the modem's 0x20 uses

# ---- WDS client (one socket = one client; the PDN lives while it is open) ----
wfd, wme = qsock()
sendto(wfd, wme.node, 0xfffffffe, struct.pack("<5I", 10, 1, 0, 0, 0))
wds, end = None, time.time() + 3
while time.time() < end and select.select([wfd], [], [], 0.5)[0]:
    cmd, svc, inst, node, port = struct.unpack_from("<5I", os.read(wfd, 4096))
    if cmd == 4 and svc == 1 and wds is None: wds = (node, port)
    if cmd == 4 and svc == 0: break
if not wds: sys.exit("no WDS service")
TXN = [0]
def wreq(mid, body, timeout=60):
    TXN[0] += 1
    sendto(wfd, wds[0], wds[1], struct.pack("<BHHH", 0, TXN[0], mid, len(body)) + body)
    end = time.time() + timeout
    while time.time() < end and select.select([wfd], [], [], 1)[0]:
        pkt = os.read(wfd, 65536); typ, tid, m, l = struct.unpack_from("<BHHH", pkt)
        if typ == 2 and tid == TXN[0] and m == mid:
            t = tlvs(pkt[7:7 + l]); res, err = struct.unpack("<HH", t[2]); return res, err, t
        if typ == 4:
            log("WDS indication 0x%02x %s" % (m, " ".join("%02x:%d" % (k, len(v)) for k, v in tlvs(pkt[7:7 + l]).items())))
    return None, None, {}

r = wreq(0xA2, tlv(0x10, struct.pack("<II", 4, 1)) + tlv(0x11, struct.pack("<B", MUX)))
log("WDS bound to mux %d: result %s error %s" % (MUX, r[0], r[1]))
PDN = {"handle": None}

def pdn_start(family, profile, apn):
    fam = 6 if family == 1 else 4
    wreq(0x4D, tlv(0x01, struct.pack("<B", fam)))
    body = tlv(0x19, struct.pack("<B", fam))
    if profile not in (0, 0xffff): body += tlv(0x31, struct.pack("<B", profile))
    if apn: body += tlv(0x14, apn.encode())
    res, err, t = wreq(0x20, body)
    if res != 0:
        reason = ""
        if 0x11 in t: reason = " verbose %d/%d" % struct.unpack("<HH", t[0x11])
        log("PDN start FAILED: result %s error %s%s" % (res, err, reason)); return None
    PDN["handle"] = t[0x01]
    res, err, s = wreq(0x2D, tlv(0x10, struct.pack("<I", 1 << 8)))
    if fam == 6 and 0x25 in s and len(s[0x25]) == 17:
        addr = socket.inet_ntop(socket.AF_INET6, s[0x25][:16])
    elif fam == 4 and 0x1e in s:
        addr = socket.inet_ntop(socket.AF_INET, s[0x1e][::-1])
    else:
        addr = None
    log("PDN up on mux %d, family %d, address %s" % (MUX, fam, (addr.split(":")[0] + ":<cut>") if addr else None))
    return addr

def pdn_stop():
    if PDN["handle"]:
        r = wreq(0x21, tlv(0x01, PDN["handle"]))
        log("PDN stopped: result %s error %s" % (r[0], r[1])); PDN["handle"] = None

# ---- IMS data server (service 770) ----
sfd, sme = qsock()
def ctrl(cmd): sendto(sfd, sme.node, 0xfffffffe, struct.pack("<5I", cmd, 770, 0 << 8 | 1, sme.node, sme.port))
CLIENTS, IND_TXN = set(), [1]
def respond(node, port, txn, mid, body): sendto(sfd, node, port, struct.pack("<BHHH", 2, txn, mid, len(body)) + body)
def indicate(mid, body):
    for node, port in CLIENTS:
        sendto(sfd, node, port, struct.pack("<BHHH", 4, IND_TXN[0], mid, len(body)) + body)
    IND_TXN[0] += 1

def voice_domain(v):
    out = subprocess.run(["/usr/bin/droid/qx1050-voice-domain.py", v], capture_output=True, text=True).stdout.strip()
    log("voice domain: %s" % out)
def imsa():
    o = subprocess.run("qmicli -d qrtr://0 --imsa-get-ims-registration-status 2>&1; qmicli -d qrtr://0 --imsa-get-ims-services-status 2>&1",
                       shell=True, capture_output=True, text=True).stdout
    return " | ".join(l.strip() for l in o.splitlines() if l.strip() and "retrieved" not in l)[:300]

ctrl(4)
log("service 770 published; running %d s" % DUR)
voice_domain("ps-preferred")
end, nextpoll, last = time.monotonic() + DUR, 0, None
try:
    while time.monotonic() < end:
        if time.monotonic() >= nextpoll:
            s = imsa()
            if s != last: log("IMSA: " + s); last = s
            nextpoll = time.monotonic() + 10
        if not select.select([sfd], [], [], 1)[0]: continue
        pkt, node, port = recvfrom(sfd)
        if port == 0xfffffffe or len(pkt) < 7: continue
        typ, txn, mid, l = struct.unpack_from("<BHHH", pkt); t = tlvs(pkt[7:7 + l])
        if typ != 0: continue
        if mid == 0x20:
            v = t.get(1, b""); n = v[0]; apn = v[1:1 + n].decode(); unk, sel, fam, i3, i2 = struct.unpack_from("<IIIHH", v, 1 + n)
            conn = struct.unpack("<I", t[0x10])[0]; sub = struct.unpack("<I", t[0x13])[0]
            log("REQ 0x20 apn=%s family=%d profile=%d conn=%d sub=%d" % (apn, fam, i3, conn, sub))
            CLIENTS.add((node, port))
            pdn_stop()
            addr = pdn_start(fam, i3, apn)
            if addr is None:
                respond(node, port, txn, mid, tlv(2, struct.pack("<HH", 1, 1))); continue
            respond(node, port, txn, mid, tlv(2, struct.pack("<HH", 0, 0)) + tlv(0x10, b"\0") +
                    tlv(0x11, struct.pack("<I", conn)) + tlv(0x12, struct.pack("<I", sub)))
            indicate(0x20, tlv(2, struct.pack("<HH", 0, 0)) + tlv(0x01, b"\0") + tlv(0x10, struct.pack("<I", conn)) +
                     tlv(0x11, struct.pack("<I", fam) + qstr(addr)) + tlv(0x12, struct.pack("<I", sub)))
            log("  answered success, address indicated")
        elif mid == 0x21:
            log("REQ 0x21 stop connection")
            pdn_stop()
            respond(node, port, txn, mid, tlv(2, struct.pack("<HH", 0, 0)) + tlv(0x10, b"\xfa") +
                    tlv(0x11, t.get(0x10, b"\0\0\0\0")[:4].ljust(4, b"\0")))
        else:
            log("REQ 0x%02x (no-op)" % mid)
            respond(node, port, txn, mid, tlv(2, struct.pack("<HH", 0, 0)))
except KeyboardInterrupt:
    pass
finally:
    ctrl(5); pdn_stop(); voice_domain("cs-preferred")
    log("IMSA at end: " + imsa())
    log("end")
