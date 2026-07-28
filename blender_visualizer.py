import bpy
import threading
import time
import math
import sys

sys.path.append(r'C:\Users\Red1 In The Ocean\AppData\Roaming\Python\Python311\site-packages')

SERIAL_PORT   = "COM3"
BAUD_RATE     = 115200
OBJECT_NAME   = "Plane"
UPDATE_HZ     = 30
GYRO_DEADBAND = 0.5
TEST_MODE     = False
RUN_DURATION  = 30
DRIFT_DECAY   = 0.999
SMOOTHING     = 0.15

_lock         = threading.Lock()
_angles_deg   = [0.0, 0.0, 0.0]
_smooth_deg   = [0.0, 0.0, 0.0]
_gyro_dps     = [0.0, 0.0, 0.0]
_temperature  = 0
_running      = False
_status       = "Idle"
_serial_conn  = None


def _stop_existing():
    global _running, _serial_conn
    _running = False
    if _serial_conn is not None:
        try:
            _serial_conn.close()
        except Exception:
            pass
        _serial_conn = None
    time.sleep(0.2)


def _parse_line(raw):
    try:
        parts = raw.strip().split(',')
        if len(parts) == 5 and parts[0] == 'G':
            return float(parts[1]), float(parts[2]), float(parts[3]), int(parts[4])
    except Exception:
        pass
    return None


def _test_worker():
    global _running, _status, _angles_deg, _gyro_dps
    _status = "TEST MODE"
    t_prev = time.perf_counter()
    while _running:
        time.sleep(1.0 / 100)
        t_now = time.perf_counter()
        dt = t_now - t_prev
        t_prev = t_now
        with _lock:
            _angles_deg[0] += 10.0 * dt
            _angles_deg[1] += 20.0 * dt
            _angles_deg[2] +=  5.0 * dt
            _gyro_dps = [20.0, 10.0, 5.0]
    _status = "Stopped"


def _serial_worker():
    global _running, _status, _angles_deg, _gyro_dps, _temperature, _serial_conn
    try:
        import serial
    except ImportError:
        _status = "ERROR: pyserial not installed"
        print("[Gyro]", _status)
        _running = False
        return
    try:
        ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1.0)
        _serial_conn = ser
        _status = f"Connected: {SERIAL_PORT}"
        print("[Gyro]", _status)
    except Exception as e:
        _status = f"SERIAL ERROR: {e}"
        print("[Gyro]", _status)
        _running = False
        return
    t_prev = time.perf_counter()
    while _running:
        try:
            raw = ser.readline().decode('utf-8', errors='ignore')
        except Exception as e:
            _status = f"Read error: {e}"
            break
        result = _parse_line(raw)
        if result is None:
            continue
        gx, gy, gz, temp = result
        gx = gx if abs(gx) >= GYRO_DEADBAND else 0.0
        gy = gy if abs(gy) >= GYRO_DEADBAND else 0.0
        gz = gz if abs(gz) >= GYRO_DEADBAND else 0.0
        t_now = time.perf_counter()
        dt = t_now - t_prev
        t_prev = t_now
        with _lock:
            _angles_deg[0] -= gy * dt
            _angles_deg[1] += gx * dt
            _angles_deg[2] += gz * dt
            _gyro_dps = [gx, gy, gz]
            _temperature = temp
            if gx == 0.0 and gy == 0.0 and gz == 0.0:
                _angles_deg[0] *= DRIFT_DECAY
                _angles_deg[1] *= DRIFT_DECAY
                _angles_deg[2] *= DRIFT_DECAY
    ser.close()
    _serial_conn = None
    _status = "Disconnected"
    print("[Gyro]", _status)


class GYRO_OT_realtime(bpy.types.Operator):
    bl_idname  = "gyro.realtime_update"
    bl_label   = "Gyro Realtime Update"
    _timer      = None
    _tick       = 0
    _start_time = 0.0

    def modal(self, context, event):
        global _running
        if not _running:
            self.cancel(context)
            return {'CANCELLED'}
        if event.type == 'ESC':
            self.cancel(context)
            return {'CANCELLED'}
        if event.type == 'TIMER':
            self._tick += 1
            elapsed = time.perf_counter() - self._start_time
            if elapsed >= RUN_DURATION:
                print(f"[Gyro] {RUN_DURATION}s elapsed - auto stopping.")
                self.cancel(context)
                return {'CANCELLED'}
            obj = bpy.data.objects.get(OBJECT_NAME)
            if obj is None:
                print(f"[Gyro] Object '{OBJECT_NAME}' not found!")
                self.cancel(context)
                return {'CANCELLED'}
            with _lock:
                tx = _angles_deg[0]
                ty = _angles_deg[1]
                tz = _angles_deg[2]

            # Low-pass filter: smooth angles drift toward target gradually
            # giving the plane a weighted, inertia-like feel
            _smooth_deg[0] += (tx - _smooth_deg[0]) * SMOOTHING
            _smooth_deg[1] += (ty - _smooth_deg[1]) * SMOOTHING
            _smooth_deg[2] += (tz - _smooth_deg[2]) * SMOOTHING

            obj.rotation_mode  = 'XYZ'
            obj.rotation_euler = (
                math.radians(_smooth_deg[0]),
                math.radians(_smooth_deg[1]),
                math.radians(_smooth_deg[2])
            )
            for area in bpy.context.screen.areas:
                if area.type == 'VIEW_3D':
                    area.tag_redraw()
            if self._tick % UPDATE_HZ == 0:
                remaining = max(0, RUN_DURATION - elapsed)
                print(f"[Gyro] Roll={_smooth_deg[1]:.1f}° Pitch={_smooth_deg[0]:.1f}° Yaw={_smooth_deg[2]:.1f}° | {_status} | {remaining:.0f}s left")
        return {'PASS_THROUGH'}

    def execute(self, context):
        global _running, _angles_deg, _smooth_deg
        _angles_deg = [0.0, 0.0, 0.0]
        _smooth_deg = [0.0, 0.0, 0.0]
        _running    = True
        obj = bpy.data.objects.get(OBJECT_NAME)
        if obj:
            obj.rotation_mode  = 'XYZ'
            obj.rotation_euler = (0.0, 0.0, 0.0)
        self._start_time = time.perf_counter()
        threading.Thread(target=_test_worker if TEST_MODE else _serial_worker, daemon=True).start()
        wm = context.window_manager
        self._timer = wm.event_timer_add(1.0 / UPDATE_HZ, window=context.window)
        wm.modal_handler_add(self)
        print(f"[Gyro] Started | TEST_MODE={TEST_MODE} | SMOOTHING={SMOOTHING}")
        return {'RUNNING_MODAL'}

    def cancel(self, context):
        global _running
        _running = False
        if self._timer:
            context.window_manager.event_timer_remove(self._timer)
        obj = bpy.data.objects.get(OBJECT_NAME)
        if obj:
            obj.rotation_euler = (0.0, 0.0, 0.0)
        print("[Gyro] Stopped - plane reset to 0,0,0")


def register():
    try:
        bpy.utils.unregister_class(GYRO_OT_realtime)
    except Exception:
        pass
    bpy.utils.register_class(GYRO_OT_realtime)


def launch():
    for window in bpy.context.window_manager.windows:
        for area in window.screen.areas:
            if area.type == 'VIEW_3D':
                for region in area.regions:
                    if region.type == 'WINDOW':
                        with bpy.context.temp_override(window=window, area=area, region=region):
                            bpy.ops.gyro.realtime_update('INVOKE_DEFAULT')
                        return


if __name__ == "__main__":
    _stop_existing()
    register()
    launch()