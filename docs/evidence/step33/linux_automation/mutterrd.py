# Wayland input injection + capture through mutter's own session-bus API
# (org.gnome.Mutter.RemoteDesktop + ScreenCast — what gnome-remote-desktop uses;
# no permission dialog for a local session client).
import gi, os, sys, time, subprocess
gi.require_version("Gio", "2.0"); gi.require_version("GLib", "2.0")
from gi.repository import Gio, GLib
BTN = {"left": 0x110, "right": 0x111, "middle": 0x112}
KEY = {"F11": 87, "Shift_L": 42, "Escape": 1, "comma": 51, "Control_L": 29}
class Mutter:
    def __init__(self, connector="HDMI-1"):
        self.bus = Gio.bus_get_sync(Gio.BusType.SESSION, None)
        rd = self._proxy("org.gnome.Mutter.RemoteDesktop", "/org/gnome/Mutter/RemoteDesktop", "org.gnome.Mutter.RemoteDesktop")
        self.rd_path = rd.call_sync("CreateSession", None, 0, 10000, None).unpack()[0]
        self.rd = self._proxy("org.gnome.Mutter.RemoteDesktop", self.rd_path, "org.gnome.Mutter.RemoteDesktop.Session")
        sid = self.rd.get_cached_property("SessionId")
        if sid is None:
            sid = self.bus.call_sync("org.gnome.Mutter.RemoteDesktop", self.rd_path, "org.freedesktop.DBus.Properties", "Get",
                                     GLib.Variant("(ss)", ("org.gnome.Mutter.RemoteDesktop.Session", "SessionId")), None, 0, 10000, None).unpack()[0]
        else: sid = sid.unpack()
        sc = self._proxy("org.gnome.Mutter.ScreenCast", "/org/gnome/Mutter/ScreenCast", "org.gnome.Mutter.ScreenCast")
        self.sc_path = sc.call_sync("CreateSession", GLib.Variant("(a{sv})", ({"remote-desktop-session-id": GLib.Variant("s", sid)},)), 0, 10000, None).unpack()[0]
        self.sc = self._proxy("org.gnome.Mutter.ScreenCast", self.sc_path, "org.gnome.Mutter.ScreenCast.Session")
        self.stream_path = self.sc.call_sync("RecordMonitor", GLib.Variant("(sa{sv})", (connector, {"cursor-mode": GLib.Variant("u", 1)})), 0, 10000, None).unpack()[0]
        self.node = None; loop = GLib.MainLoop()
        def on_added(conn, sender, path, iface, signal, params): self.node = params.unpack()[0]; loop.quit()
        self.bus.signal_subscribe("org.gnome.Mutter.ScreenCast", "org.gnome.Mutter.ScreenCast.Stream", "PipeWireStreamAdded", self.stream_path, None, 0, on_added)
        self.rd.call_sync("Start", None, 0, 10000, None)   # starts the linked screencast too
        GLib.timeout_add_seconds(10, loop.quit); loop.run()
        params = self.bus.call_sync("org.gnome.Mutter.ScreenCast", self.stream_path, "org.freedesktop.DBus.Properties", "Get",
                                    GLib.Variant("(ss)", ("org.gnome.Mutter.ScreenCast.Stream", "Parameters")), None, 0, 10000, None).unpack()[0]
        self.size = tuple(params.get("size", (0, 0))); self.position = tuple(params.get("position", (0, 0)))
        print("mutter: stream", self.stream_path, "node", self.node, "size", self.size, "position", self.position)
    def _proxy(self, name, path, iface):
        return Gio.DBusProxy.new_sync(self.bus, Gio.DBusProxyFlags.NONE, None, name, path, iface, None)
    def move(self, x, y): self.rd.call_sync("NotifyPointerMotionAbsolute", GLib.Variant("(sdd)", (self.stream_path, float(x), float(y))), 0, 5000, None)
    def button(self, b, down): self.rd.call_sync("NotifyPointerButton", GLib.Variant("(ib)", (BTN[b], bool(down))), 0, 5000, None)
    def key(self, k, down): self.rd.call_sync("NotifyKeyboardKeycode", GLib.Variant("(ub)", (KEY[k], bool(down))), 0, 5000, None)
    def click(self, x, y, b="left", hold=0.08):
        self.move(x, y); time.sleep(0.05); self.button(b, True); time.sleep(hold); self.button(b, False)
    def drag(self, b, x0, y0, x1, y1, secs, steps=40, shift=False):
        self.move(x0, y0); time.sleep(0.1)
        if shift: self.key("Shift_L", True); time.sleep(0.05)
        self.button(b, True); time.sleep(0.05)
        for i in range(1, steps + 1):
            self.move(x0 + (x1 - x0) * i / steps, y0 + (y1 - y0) * i / steps); time.sleep(secs / steps)
        self.button(b, False)
        if shift: self.key("Shift_L", False)
    def hold(self, b, x, y, secs, shift=False):
        self.move(x, y); time.sleep(0.1)
        if shift: self.key("Shift_L", True); time.sleep(0.05)
        self.button(b, True); time.sleep(secs); self.button(b, False)
        if shift: self.key("Shift_L", False)
    def tap(self, k): self.key(k, True); time.sleep(0.06); self.key(k, False)
    def shot(self, path):
        cmd = ["gst-launch-1.0", "-q", "pipewiresrc", f"path={self.node}", "num-buffers=3", "!", "videoconvert", "!",
               "video/x-raw,format=RGB", "!", "pngenc", "snapshot=true", "!", "filesink", f"location={path}"]
        r = subprocess.run(cmd, capture_output=True, text=True, timeout=30)
        return os.path.exists(path) and os.path.getsize(path) > 0, r.stderr[-300:]
    def stop(self):
        try: self.rd.call_sync("Stop", None, 0, 5000, None)
        except Exception as e: print("stop:", e)
if __name__ == "__main__":
    m = Mutter(sys.argv[2] if len(sys.argv) > 2 else "HDMI-1"); ok, err = m.shot(sys.argv[1]); print("shot:", ok, err)
    m.move(100, 100); print("pointer moved (no error)"); m.stop()
