#!/usr/bin/env python3
"""Serve the wasm build over HTTPS for LAN / device testing (Phase 5 §5).

  python3 tools/web_serve.py [--dist build-web/web-dist] [--port 8443] [--http 8765]

WebGPU (and WebMIDI) exist only in SECURE CONTEXTS: https:// or localhost.
`python3 -m http.server` is fine on this machine (localhost is secure), but an
iPad or another PC reaching the Mac by LAN IP over plain http sees
`navigator.gpu === undefined`. This serves the same directory over HTTPS with a
self-signed certificate (generated once with the system openssl into
<dist>/../devcert/, SANs = this machine's addresses); the browser warns about
the certificate once — proceed (Safari: "Show details → visit this website";
Chrome: type "thisisunsafe" on the warning page). Plain HTTP stays on --http for
localhost. Python, not Node, on purpose: the macOS firewall on the author's Mac
blocks incoming connections for Homebrew's node binary and allows python3.
"""
import argparse
import http.server
import os
import socket
import ssl
import subprocess
import sys
import threading


def lan_ips():
    ips = set()
    try:
        for info in socket.getaddrinfo(socket.gethostname(), None, socket.AF_INET):
            ips.add(info[4][0])
    except socket.gaierror:
        pass
    # The interface actually used to reach the outside world.
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.connect(("10.255.255.255", 1))
        ips.add(s.getsockname()[0])
        s.close()
    except OSError:
        pass
    return sorted(ip for ip in ips if not ip.startswith("127."))


def ensure_cert(cert_dir, ips):
    key, crt = os.path.join(cert_dir, "key.pem"), os.path.join(cert_dir, "cert.pem")
    if os.path.exists(key) and os.path.exists(crt):
        return key, crt
    os.makedirs(cert_dir, exist_ok=True)
    san = ",".join(["DNS:localhost", "IP:127.0.0.1"] + [f"IP:{ip}" for ip in ips])
    r = subprocess.run(["openssl", "req", "-x509", "-newkey", "rsa:2048", "-nodes", "-days", "365",
                        "-keyout", key, "-out", crt, "-subj", "/CN=midi-sink dev",
                        "-addext", f"subjectAltName={san}"], capture_output=True, text=True)
    if r.returncode != 0:
        sys.exit(f"openssl failed: {r.stderr}")
    print(f"self-signed dev certificate written to {cert_dir} (SAN: {san})")
    return key, crt


class Handler(http.server.SimpleHTTPRequestHandler):
    extensions_map = {**http.server.SimpleHTTPRequestHandler.extensions_map,
                      ".wasm": "application/wasm", ".js": "text/javascript", ".mjs": "text/javascript"}

    def end_headers(self):
        self.send_header("Cache-Control", "no-store")
        super().end_headers()

    def log_message(self, fmt, *args):   # one quiet line per request
        sys.stderr.write("  %s %s\n" % (self.address_string(), fmt % args))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--dist", default="build-web/web-dist")
    ap.add_argument("--port", type=int, default=8443)
    ap.add_argument("--http", type=int, default=8765)
    a = ap.parse_args()
    dist = os.path.abspath(a.dist)
    if not os.path.isdir(dist):
        sys.exit(f"{dist} not found — build with `emcmake cmake -B build-web -G Ninja && cmake --build build-web`")
    ips = lan_ips()
    key, crt = ensure_cert(os.path.join(os.path.dirname(dist), "devcert"), ips)

    handler = lambda *args, **kw: Handler(*args, directory=dist, **kw)   # noqa: E731
    ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    ctx.load_cert_chain(crt, key)
    # One HTTPS socket PER ADDRESS, not a wildcard bind: on the author's Mac the
    # application firewall resets LAN connections to a 0.0.0.0-bound socket
    # while an explicit LAN-address bind is reachable (measured).
    servers = []
    for ip in ["127.0.0.1"] + ips:
        try:
            srv = http.server.ThreadingHTTPServer((ip, a.port), handler)
        except OSError as e:
            print(f"  (skip https on {ip}: {e})")
            continue
        srv.socket = ctx.wrap_socket(srv.socket, server_side=True)
        servers.append(srv)
    http_srv = http.server.ThreadingHTTPServer(("127.0.0.1", a.http), handler)

    print(f"serving {dist}")
    print(f"  this machine:  http://localhost:{a.http}/   (localhost is a secure context)")
    for ip in ips:
        print(f"  LAN / iPad:    https://{ip}:{a.port}/   (accept the self-signed certificate once;"
              f" if unreachable, allow Python in System Settings > Network > Firewall)")
    threading.Thread(target=http_srv.serve_forever, daemon=True).start()
    for srv in servers[1:]:
        threading.Thread(target=srv.serve_forever, daemon=True).start()
    try:
        servers[0].serve_forever()
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main()
