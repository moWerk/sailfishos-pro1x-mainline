#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""qx1050-imsd: IMS data daemon for VoLTE on the mainline Pro1-X (prototype).

The modem runs the IMS stack (SIP, registration, calls, RTP) itself. It asks the host for the
IMS data connection through QMI service 770 ("IMS data", v1), as 81voltd (Richard Acayan,
GPL-2.0-or-later; message layout from its imsd.qmi) serves it through ModemManager. Here the
connection is an oFono context of type "ims"; oFono patch 0022 adds the modem's stored profile
index for the APN, without which the network refuses the IMS PDN.

- voice domain ps-preferred while running (the modem asks for IMS only then), cs-preferred on
  exit (also ExecStopPost in the unit);
- Start Connection (0x20): activate the ims context (APN, family as requested), answer, report
  the address (indication 0x20); Stop Connection (0x21): deactivate;
- the context dropping is reported as down;
- watchdog (after kabouik's pro1x-volte-watch): on LTE without an active ims context for 30 s,
  publish the service again so the modem asks again; back off 90 s doubling to 30 min.
Talks to oFono with dbus-send (no Python D-Bus bindings on the image). Addresses are logged
by prefix only.
"""
import ctypes, os, re, select, signal, socket, struct, subprocess, sys, time

SERVICE, VERSION = 770, 1
CTRL_PORT = 0xfffffffe
VOICE_DOMAIN = "/usr/bin/droid/qx1050-voice-domain.py"

def log(m): print(m, flush=True)
def sh(cmd):
    return subprocess.run(cmd, shell=True, capture_output=True, text=True).stdout

# ---------------------------------------------------------------- oFono via dbus-send
DS = "dbus-send --system --print-reply --dest=org.ofono"
PROP = re.compile(r'string "([A-Za-z.]+)"\s+variant\s+(?:string "([^"]*)"|boolean (\w+))')
def modem_path():
    m = re.search(r'object path "(/[^"]+)"', sh(DS + " / org.ofono.Manager.GetModems"))
    return m.group(1) if m else None

def contexts(modem):
    """{path: {prop: value}} for the modem's contexts (string/boolean properties only)."""
    out = sh("%s %s org.ofono.ConnectionManager.GetContexts" % (DS, modem))
    res = {}
    for part in out.split('object path "')[1:]:
        path = part.split('"', 1)[0]
        res[path] = {k: a or b for k, a, b in PROP.findall(part)}
    return res

def ctx_props(path):
    out = sh("%s %s org.ofono.ConnectionContext.GetProperties" % (DS, path))
    props = {k: a or b for k, a, b in PROP.findall(out.split('"Settings"')[0])}
    v6 = out.split('"IPv6.Settings"', 1)
    if len(v6) == 2:
        m = re.search(r'string "Address"\s+variant\s+string "([^"]+)"', v6[1])
        if m: props["IPv6.Address"] = m.group(1)
    v4 = out.split('"Settings"', 1)
    if len(v4) == 2:
        m = re.search(r'string "Address"\s+variant\s+string "([^"]+)"', v4[1].split('"IPv6.Settings"')[0])
        if m: props["IPv4.Address"] = m.group(1)
    return props

def ctx_set(path, prop, sig):
    out = subprocess.run("%s %s org.ofono.ConnectionContext.SetProperty string:%s variant:%s" % (DS, path, prop, sig),
                         shell=True, capture_output=True, text=True)
    return out.returncode == 0, (out.stderr or out.stdout).strip().splitlines()[-1:]

def ims_context(modem, create):
    for path, p in contexts(modem).items():
        if p.get("Type") == "ims":
            return path
    if not create:
        return None
    m = re.search(r'object path "(/[^"]+)"', sh("%s %s org.ofono.ConnectionManager.AddContext string:ims" % (DS, modem)))
    return m.group(1) if m else None

def technology(modem):
    m = re.search(r'"Technology"\s+variant\s+string "([^"]*)"', sh("%s %s org.ofono.NetworkRegistration.GetProperties" % (DS, modem)))
    return m.group(1) if m else ""

def short(addr): return (addr.split(":")[0] + ":<cut>") if addr and ":" in addr else ("<v4>" if addr else None)

def voice_domain(v):
    log("voice domain %s: %s" % (v, sh("%s %s" % (VOICE_DOMAIN, v)).strip()))

# ---------------------------------------------------------------- QRTR / QMI
libc = ctypes.CDLL(None, use_errno=True)
class SA(ctypes.Structure):
    _fields_ = [("family", ctypes.c_ushort), ("node", ctypes.c_uint), ("port", ctypes.c_uint)]
fd = libc.socket(42, socket.SOCK_DGRAM, 0)
if fd < 0: sys.exit("no QRTR socket")
me = SA(); ln = ctypes.c_uint(ctypes.sizeof(me)); libc.getsockname(fd, ctypes.byref(me), ctypes.byref(ln))
def sendto(node, port, pkt):
    d = SA(42, node, port); b = ctypes.create_string_buffer(pkt, len(pkt))
    libc.sendto(fd, b, len(pkt), 0, ctypes.byref(d), ctypes.sizeof(d))
def recvfrom():
    buf = ctypes.create_string_buffer(65536); src = SA(); sl = ctypes.c_uint(ctypes.sizeof(src))
    n = libc.recvfrom(fd, buf, 65536, 0, ctypes.byref(src), ctypes.byref(sl))
    return buf.raw[:max(n, 0)], src.node, src.port
tlv = lambda t, d: struct.pack("<BH", t, len(d)) + d
def tlvs(body):
    out, i = {}, 0
    while i + 3 <= len(body):
        t, l = struct.unpack_from("<BH", body, i); out[t] = body[i + 3:i + 3 + l]; i += 3 + l
    return out
def publish(on): sendto(me.node, CTRL_PORT, struct.pack("<5I", 4 if on else 5, SERVICE, VERSION, me.node, me.port))
def respond(node, port, txn, mid, body): sendto(node, port, struct.pack("<BHHH", 2, txn, mid, len(body)) + body)
OK = tlv(2, struct.pack("<HH", 0, 0))

class State:
    clients = set()
    conn = None          # (orig conn id, subscription, family)
    ind_txn = 1
    path = None
    up = False
S = State()

def indicate(addr):
    conn, sub, fam = S.conn
    if addr:
        body = OK + tlv(1, b"\0") + tlv(0x10, struct.pack("<I", conn)) + \
               tlv(0x11, struct.pack("<I", fam) + struct.pack("<B", len(addr)) + addr.encode()) + tlv(0x12, struct.pack("<I", sub))
    else:
        body = tlv(2, struct.pack("<HH", 0, 13)) + tlv(1, b"\0") + tlv(0x10, struct.pack("<I", conn)) + tlv(0x12, struct.pack("<I", sub))
    for node, port in S.clients:
        sendto(node, port, struct.pack("<BHHH", 4, S.ind_txn, 0x20, len(body)) + body)
    S.ind_txn = (S.ind_txn + 1) & 0xffff

def start_connection(modem, apn, fam):
    path = ims_context(modem, True)
    if not path:
        log("no ims context could be created"); return None
    S.path = path
    p = ctx_props(path)
    if p.get("Active") == "true" and p.get("AccessPointName") == apn:
        pass
    else:
        if p.get("Active") == "true":
            ctx_set(path, "Active", "boolean:false")
        ctx_set(path, "AccessPointName", "string:%s" % apn)
        ctx_set(path, "Protocol", "string:%s" % ("ipv6" if fam == 1 else "ip"))
        ok, msg = ctx_set(path, "Active", "boolean:true")
        if not ok:
            log("activating %s failed: %s" % (path, " ".join(msg))); return None
    for _ in range(20):
        p = ctx_props(path)
        addr = p.get("IPv6.Address" if fam == 1 else "IPv4.Address")
        if p.get("Active") == "true" and addr:
            return addr
        time.sleep(0.5)
    log("context active but no address"); return None

def stop_connection():
    if S.path:
        ctx_set(S.path, "Active", "boolean:false")
    S.up = False

def handle(modem, pkt, node, port):
    typ, txn, mid, l = struct.unpack_from("<BHHH", pkt); t = tlvs(pkt[7:7 + l])
    if typ != 0: return
    if mid == 0x20:
        v = t.get(1, b"")
        n = v[0] if v else 0
        apn = v[1:1 + n].decode("ascii", "replace")
        _, _, fam, idx, _ = struct.unpack_from("<IIIHH", v, 1 + n) if len(v) >= 1 + n + 16 else (0, 0, 1, 0, 0)
        conn = struct.unpack("<I", t[0x10])[0] if len(t.get(0x10, b"")) == 4 else 0
        sub = struct.unpack("<I", t[0x13])[0] if len(t.get(0x13, b"")) == 4 else 1
        log("modem asks: APN %s, %s, profile %d" % (apn, "IPv6" if fam == 1 else "IPv4", idx))
        S.clients.add((node, port)); S.conn = (conn, sub, fam)
        addr = start_connection(modem, apn, fam)
        if not addr:
            respond(node, port, txn, mid, tlv(2, struct.pack("<HH", 1, 1))); return
        respond(node, port, txn, mid, OK + tlv(0x10, b"\0") + tlv(0x11, struct.pack("<I", conn)) + tlv(0x12, struct.pack("<I", sub)))
        indicate(addr); S.up = True
        log("IMS connection up (%s), address reported" % short(addr))
    elif mid == 0x21:
        log("modem: stop connection")
        stop_connection()
        respond(node, port, txn, mid, OK + tlv(0x10, b"\xfa") + tlv(0x11, t.get(0x10, b"\0\0\0\0")[:4].ljust(4, b"\0")))
    else:
        respond(node, port, txn, mid, OK)

# ---------------------------------------------------------------- main loop
RUN = [True]
def stop(*_): RUN[0] = False
signal.signal(signal.SIGTERM, stop); signal.signal(signal.SIGINT, stop)

def main():
    modem = None
    for _ in range(60):
        modem = modem_path()
        if modem: break
        time.sleep(2)
    if not modem: sys.exit("no oFono modem")
    log("modem %s" % modem)
    voice_domain("ps-preferred")
    publish(True)
    log("serving QMI %d" % SERVICE)
    missing_since, backoff, next_kick, next_check = None, 90, 0, time.monotonic() + 5
    while RUN[0]:
        if select.select([fd], [], [], 1)[0]:
            pkt, node, port = recvfrom()
            if port != CTRL_PORT and len(pkt) >= 7:
                handle(modem, pkt, node, port)
            continue
        now = time.monotonic()
        if now < next_check: continue
        next_check = now + 5
        p = ctx_props(S.path) if S.path else {}
        active = p.get("Active") == "true"
        if S.up and not active:
            log("IMS connection dropped"); S.up = False
            if S.conn: indicate(None)
        if active or technology(modem) != "lte":
            missing_since = None
            if active: backoff = 90
            continue
        missing_since = missing_since or now
        if now - missing_since >= 30 and now >= next_kick:
            log("on LTE without IMS connection for %d s: asking the modem again (next try in %d s)" % (now - missing_since, backoff))
            voice_domain("ps-preferred")
            publish(False); time.sleep(1); publish(True)
            next_kick = now + backoff; backoff = min(backoff * 2, 1800)
    log("stopping")
    publish(False)
    stop_connection()
    voice_domain("cs-preferred")

main()
