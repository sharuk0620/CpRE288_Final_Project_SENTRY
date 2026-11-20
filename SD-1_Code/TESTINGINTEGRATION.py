import tkinter as tk
import threading
import queue
import socket
import os
import numpy as np
import matplotlib
matplotlib.use("TkAgg")
from matplotlib.figure import Figure
from matplotlib.patches import Circle
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg

# ---------- Settings ----------
HOST = "192.168.1.1"   # same IP you use in PuTTY
PORT = 288             # same port you use in PuTTY

# What each button sends to the CyBot (CHANGE these to match your main.c!)
CMD_FORWARD       = "w\n"
CMD_BACKWARD      = "s\n"
CMD_LEFT          = "a\n"
CMD_RIGHT         = "d\n"
CMD_SCAN          = "m\n"
CMD_TURN_LEFT_90  = "q\n"   # Turn 90 Left
CMD_TURN_RIGHT_90 = "e\n"   # Turn 90 Right
CMD_CONTINUE      = "h\n"   # continue in autonomous
CMD_TOGGLE        = "t\n"   # switch between auto and manual
CMD_QUIT          = "quit\n"

mode = "MANUAL"             # current mode (GUI side). CyBot should boot in MANUAL.

filename = "sensor-scan.txt"
full_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), filename)

# Colors
BG = "black"
FG = "lime"      # green for the plot ("#00ff00")
PLAN_FG = "yellow"

# ---------- Arena / map settings (all in cm) ----------
# Arena is 8 x 4 tiles of 24 in (60.96 cm) each, so ~487.7 x ~243.8 cm.
# Edit these if you measure the real arena.
TILE_CM = 24 * 2.54
ARENA_LONG_CM = 8 * TILE_CM
ARENA_SHORT_CM = 4 * TILE_CM
# Longest possible reach from the CyBot's start corner to any point in the
# arena: the arena's diagonal (~545 cm).
ARENA_DIAG_CM = float(np.hypot(ARENA_LONG_CM, ARENA_SHORT_CM))
MAP_LIM_CM = 600                      # map shows -600 to 600 cm in x and y
LABEL_CM = 100                        # axis numbers every 100 cm
MAP_FIGSIZE = (8, 8)                  # make bigger/smaller to resize the map
START_HEADING_DEG = 90.0              # heading assumed until the first POS line arrives
OBJECT_MERGE_CM = 10.0                # same object seen again within this -> one dot
TYPE_COLORS = {"SMALL": "#00ffff", "MEDIUM": "#ffff00", "LARGE": "#ff8800",
               "BUMPER": "#ff00ff", "CLIFF": "#ff0000", "WALL": "#ffffff"}

# GUI thread puts commands here, sender thread takes them out
command_queue = queue.Queue()

# Every object seen this run (kept for the map graph you'll build later).
# Each entry: {"angle", "dist", "width", "x", "y", "type"} (unused ones are None)
map_objects = []

# CyBot pose on the map, in cm and degrees (0 deg = +x, 90 deg = +y, CCW positive)
robot_pose = {"x": 0.0, "y": 0.0, "h": START_HEADING_DEG}
robot_path = [(0.0, 0.0)]             # where the CyBot has been

window = tk.Tk()
window.title("CyBot Control")
window.configure(bg=BG)

# StringVar must be created AFTER the root window exists
mode_var = tk.StringVar(value="Mode: MANUAL")

# ---------- Labels ----------
mode_label = tk.Label(window, textvariable=mode_var, font=("Work Sans", 10),
                      bg=BG, fg=FG)
mode_label.grid(row=3, column=0)

message_label = tk.Label(window, text="", font=("Times New Roman", 12), bg=BG, fg=FG)
message_label.grid(row=3, column=1)

# ---------- Button actions ----------
def manual_command(text, cmd):
    """Manual-only commands are ignored while in AUTO."""
    if mode != "MANUAL":
        return
    message_label.config(text=text)
    command_queue.put(cmd)

def buttonForward():     manual_command("MOVING FORWARD",   CMD_FORWARD)
def buttonBackward():    manual_command("MOVING BACKWARD",  CMD_BACKWARD)
def buttonLeft():        manual_command("MOVING LEFTWARD",  CMD_LEFT)
def buttonRight():       manual_command("MOVING RIGHTWARD", CMD_RIGHT)
def buttonScan():        manual_command("SCANNING",         CMD_SCAN)
def buttonTurnLeft90():  manual_command("TURNING 90 LEFT",  CMD_TURN_LEFT_90)
def buttonTurnRight90(): manual_command("TURNING 90 RIGHT", CMD_TURN_RIGHT_90)

def buttonContinue():
    """Continue only makes sense in AUTO."""
    if mode != "AUTO":
        return
    message_label.config(text="CONTINUING")
    command_queue.put(CMD_CONTINUE)

def toggleMode():
    global mode
    mode = "AUTO" if mode == "MANUAL" else "MANUAL"
    mode_var.set(f"Mode: {mode}")
    message_label.config(text="TOGGLE")
    command_queue.put(CMD_TOGGLE)
    update_buttons()

def apply_mode(new_mode):
    """Optional: if the robot prints 'MODE AUTO' / 'MODE MANUAL', trust it."""
    global mode
    mode = new_mode
    mode_var.set(f"Mode: {mode}")
    update_buttons()

def on_close():
    command_queue.put(CMD_QUIT)
    window.after(500, window.destroy)   # give the thread a moment to send quit

window.protocol("WM_DELETE_WINDOW", on_close)

# ---------- Buttons ----------
btn_style = dict(width=25, height=5, bg="black", fg="red",
                 activebackground="#222222", activeforeground="red",
                 disabledforeground="#550000")

accessory_style = dict(width=16, height=2, bg="#3a3a3a", fg="red",
                       activebackground="#555555", activeforeground="red",
                       disabledforeground="#550000", font=("Arial", 9))

btn_turn_left_90 = tk.Button(window, text="TURN 90 LEFT", command=buttonTurnLeft90, **accessory_style)
btn_turn_left_90.grid(row=0, column=0)

btn_forward = tk.Button(window, text="FORWARD", command=buttonForward, **btn_style)
btn_forward.grid(row=0, column=1)

btn_turn_right_90 = tk.Button(window, text="TURN 90 RIGHT", command=buttonTurnRight90, **accessory_style)
btn_turn_right_90.grid(row=0, column=2)

btn_left = tk.Button(window, text="LEFTWARD", command=buttonLeft, **btn_style)
btn_left.grid(row=1, column=0)

btn_scan = tk.Button(window, text="SCAN", command=buttonScan, **btn_style)
btn_scan.grid(row=1, column=1)

btn_right = tk.Button(window, text="RIGHTWARD", command=buttonRight, **btn_style)
btn_right.grid(row=1, column=2)

btn_toggle = tk.Button(window, text="TOGGLE", command=toggleMode, **accessory_style)
btn_toggle.grid(row=2, column=0)

btn_backward = tk.Button(window, text="BACKWARD", command=buttonBackward, **btn_style)
btn_backward.grid(row=2, column=1)

btn_continue = tk.Button(window, text="CONTINUE", command=buttonContinue, **accessory_style)
btn_continue.grid(row=2, column=2)

manual_buttons = [btn_turn_left_90, btn_forward, btn_turn_right_90,
                  btn_left, btn_scan, btn_right, btn_backward]

def update_buttons():
    """Grey out whatever doesn't apply to the current mode."""
    manual_state = "normal" if mode == "MANUAL" else "disabled"
    auto_state   = "normal" if mode == "AUTO"   else "disabled"
    for b in manual_buttons:
        b.config(state=manual_state)
    btn_continue.config(state=auto_state)

update_buttons()

# ---------- Polar plot embedded in the window ----------
fig = Figure(figsize=(5, 4), facecolor=BG)
ax = fig.add_subplot(111, projection="polar")
canvas = FigureCanvasTkAgg(fig, master=window)
canvas.get_tk_widget().configure(bg=BG, highlightthickness=0)
canvas.get_tk_widget().grid(row=4, column=0, columnspan=3)

# ---------- Objects list + plan under the plot ----------
objects_label = tk.Label(window, text="Objects detected: (no scan yet)",
                         font=("Courier New", 12), bg=BG, fg=FG, justify="left")
objects_label.grid(row=5, column=0, columnspan=3, pady=(5, 5))

plan_label = tk.Label(window, text="Plan: (none)", font=("Courier New", 12),
                      bg=BG, fg=PLAN_FG, justify="left")
plan_label.grid(row=6, column=0, columnspan=3, pady=(0, 10))

def show_objects(objects):
    """Update the object text under the plot (runs in the GUI thread)."""
    if not objects:
        objects_label.config(text="Objects detected: 0")
        return
    blocks = ["Objects detected: " + str(len(objects))]
    for i, o in enumerate(objects, start=1):
        lines = ["Object %d: distance %.1f cm" % (i, o["dist"]),
                 "angle: %.1f deg" % o["angle"]]
        # Only shown when the robot sends them (not in the original format)
        if o.get("width") is not None:
            lines.append("width: %.2f cm" % o["width"])
        if o.get("type") is not None:
            lines.append("type: %s" % o["type"].lower())
        if o.get("x") is not None:
            lines.append("x-coord: %.1f cm" % o["x"])
        if o.get("y") is not None:
            lines.append("y-coord: %.1f cm" % o["y"])
        blocks.append("\n".join(lines))
    objects_label.config(text="\n\n".join(blocks))

def show_plan(text):
    plan_label.config(text=text)

def setup_axes():
    """0-180 degree half-circle setup, styled green on black."""
    ax.set_facecolor(BG)
    ax.set_thetamin(0)
    ax.set_thetamax(180)
    ax.set_rmax(100)
    ax.set_rticks([20, 40, 60, 80, 100])
    ax.set_rlabel_position(-22.5)
    ax.set_xticks(np.arange(0, np.pi + .1, np.pi / 4))
    ax.tick_params(axis='both', which='major', labelsize=14, colors=FG)
    ax.grid(True, color=FG, alpha=0.5)
    for spine in ax.spines.values():
        spine.set_edgecolor(FG)
    ax.set_title("CyBot Sensor Scan from 0 to 180 Degrees", size=14, y=1.0, pad=-24, color=FG)

setup_axes()
canvas.draw()

# ---------- Big map grid (right of everything) ----------
# The CyBot's start point is (0, 0) in the middle. Because we don't know where
# in the arena it starts, the circle has a radius equal to the arena diagonal,
# so the whole arena fits inside it no matter where the CyBot began.
map_fig = Figure(figsize=MAP_FIGSIZE, facecolor=BG)
map_ax = map_fig.add_subplot(111)
map_canvas = FigureCanvasTkAgg(map_fig, master=window)
map_canvas.get_tk_widget().configure(bg=BG, highlightthickness=0)
map_canvas.get_tk_widget().grid(row=0, column=3, rowspan=7, padx=10)

def setup_map():
    """Draw the empty circular tile grid."""
    map_ax.clear()
    map_ax.set_facecolor(BG)
    lim = MAP_LIM_CM
    map_ax.set_xlim(-lim, lim)
    map_ax.set_ylim(-lim, lim)
    map_ax.set_aspect("equal")

    # Outer circle = arena diagonal
    map_ax.add_patch(Circle((0, 0), ARENA_DIAG_CM, fill=False,
                            edgecolor=FG, linewidth=2))

    ticks = np.arange(-MAP_LIM_CM, MAP_LIM_CM + 1, LABEL_CM)
    map_ax.set_xticks(ticks)
    map_ax.set_yticks(ticks)
    map_ax.tick_params(colors=FG, labelsize=8)
    map_ax.set_xlabel("x (cm)", color=FG)
    map_ax.set_ylabel("y (cm)", color=FG)
    for spine in map_ax.spines.values():
        spine.set_edgecolor(FG)
    map_ax.set_title("Arena Map (cm; red line = path, purple dot = CyBot)", color=FG, size=12)

def draw_map():
    """Redraw the grid, the CyBot's path, the CyBot, and every object seen."""
    setup_map()

    # Path the CyBot has driven
    if len(robot_path) > 1:
        xs, ys = zip(*robot_path)
        map_ax.plot(xs, ys, color="red", linewidth=0.6, zorder=2)

    # Objects, colored by type (type is "OBJECT" until the robot sends one)
    by_type = {}
    for o in list(map_objects):
        if o.get("x") is None or o.get("y") is None:
            continue
        t = (o.get("type") or "OBJECT").upper()
        by_type.setdefault(t, []).append((o["x"], o["y"]))
    for t, pts in by_type.items():
        xs, ys = zip(*pts)
        map_ax.scatter(xs, ys, s=70, color=TYPE_COLORS.get(t, "white"),
                       label=t, zorder=3)
    if by_type:
        leg = map_ax.legend(loc="upper right", facecolor=BG, edgecolor=FG,
                            labelcolor=FG, fontsize=9)
        leg.set_zorder(8)

    # CyBot's current position: purple dot
    map_ax.plot(robot_pose["x"], robot_pose["y"], "o", color="#8000ff",
                markersize=12, zorder=5)
    map_canvas.draw()

def apply_pose(x, y, h):
    """New CyBot position from a POS line (runs in the GUI thread)."""
    robot_pose["x"], robot_pose["y"], robot_pose["h"] = x, y, h
    if (x, y) != robot_path[-1]:
        robot_path.append((x, y))
    draw_map()

draw_map()

def plot_scan(rows):
    """rows = list of (angle, raw, smoothed). Plots from memory (runs in GUI thread)."""
    if not rows:
        return
    angle_radians = np.radians([r[0] for r in rows])
    distance = [r[2] for r in rows]
    ax.clear()
    ax.plot(angle_radians, distance, color=FG, linewidth=4.0)
    setup_axes()
    canvas.draw()

def save_scan(rows):
    """Still write the file for your records, but nothing reads it back."""
    try:
        with open(full_path, "w") as f:
            f.write("Angle Raw Smoothed\n")
            for a, r, s in rows:
                f.write("%s %s %s\n" % (a, r, s))
    except OSError:
        pass

# ---------- Socket threads ----------
def set_label(text):
    window.after(0, lambda: message_label.config(text=text))

def sender_thread(cybot_socket):
    """Sends whatever the buttons queue up. Never reads, so it can never get stuck."""
    while True:
        send_message = command_queue.get()
        try:
            cybot_socket.sendall(send_message.encode())
        except OSError:
            break
        print("Sent to server: " + send_message.strip())
        if send_message == CMD_QUIT:
            break
    try:
        cybot_socket.close()
    except OSError:
        pass

def receiver_thread(cybot_socket):
    """Always listening, so scan data is handled in AUTO or MANUAL."""
    cybot = cybot_socket.makefile("rb")
    scanning = False
    scan_rows = []
    objects = []
    pose = [0.0, 0.0, START_HEADING_DEG]   # x cm, y cm, heading deg

    def begin_scan():
        nonlocal scanning, scan_rows, objects
        scanning = True
        scan_rows = []
        objects = []
        window.after(0, lambda: objects_label.config(text="Objects detected: scanning..."))
        window.after(0, lambda: show_plan("Plan: (waiting)"))
        set_label("SCANNING")

    try:
        while True:
            raw = cybot.readline()
            if raw == b"":                              # connection closed
                break
            line = raw.decode(errors="ignore").strip()
            if not line:
                continue
            print(line)
            upper = line.upper()
            parts = line.split()

            # Optional: robot reports its mode ("MODE AUTO" / "MODE MANUAL")
            if len(parts) == 2 and parts[0].upper() == "MODE":
                new_mode = parts[1].upper()
                if new_mode in ("AUTO", "MANUAL"):
                    window.after(0, lambda m=new_mode: apply_mode(m))
                continue

            # Position line from the robot: POS x_mm y_mm heading_deg
            if parts[0].upper() == "POS" and len(parts) >= 4:
                try:
                    px = float(parts[1]) / 10.0      # mm -> cm
                    py = float(parts[2]) / 10.0
                    ph = float(parts[3])
                except ValueError:
                    continue
                pose[0], pose[1], pose[2] = px, py, ph
                window.after(0, lambda a=px, b=py, c=ph: apply_pose(a, b, c))
                continue

            # Start of a scan: reset everything from the previous scan
            if "STARTSCAN" in upper:
                begin_scan()
                continue

            # End of a scan: save + plot from memory (accepts either marker)
            if "ENDSCAN" in upper or "SCAN DONE" in upper:
                if scanning:
                    scanning = False
                    rows = list(scan_rows)
                    save_scan(rows)
                    window.after(0, lambda r=rows: plot_scan(r))
                    window.after(0, lambda: show_objects([]))
                set_label("SCAN COMPLETE")
                continue

            # Object line (arrives after ENDSCAN). Original format:
            #   OBJECT angle dist width
            # Later format (not used yet): OBJECT angle dist x y TYPE
            if parts[0].upper() == "OBJECT" and len(parts) >= 4:
                try:
                    obj = {"angle": float(parts[1]),
                           "dist":  float(parts[2]),
                           "width": None, "x": None, "y": None, "type": None}
                    if len(parts) >= 6:
                        obj["x"] = float(parts[3])
                        obj["y"] = float(parts[4])
                        obj["type"] = parts[5]
                    else:
                        obj["width"] = float(parts[3])
                except ValueError:
                    continue
                if obj["x"] is None:
                    # Scan angle 90 = straight ahead; angles above 90 are to the left (CCW)
                    bearing = np.radians(pose[2] + (obj["angle"] - 90.0))
                    obj["x"] = pose[0] + obj["dist"] * float(np.cos(bearing))
                    obj["y"] = pose[1] + obj["dist"] * float(np.sin(bearing))
                objects.append(obj)
                for i, m in enumerate(map_objects):       # same object seen again?
                    if m["x"] is not None and np.hypot(m["x"] - obj["x"],
                                                       m["y"] - obj["y"]) < OBJECT_MERGE_CM:
                        map_objects[i] = obj
                        break
                else:
                    map_objects.append(obj)
                window.after(0, draw_map)
                window.after(0, lambda objs=list(objects): show_objects(objs))
                continue

            # Plan lines: "PLAN turn 61 move 0.4" or "PLAN search turn 90"
            if parts[0].upper() == "PLAN":
                if len(parts) >= 5 and parts[1].lower() == "turn" and parts[3].lower() == "move":
                    text = "Plan: turn %s deg, move %s cm" % (parts[2], parts[4])
                elif len(parts) >= 4 and parts[1].lower() == "search":
                    text = "Plan: no objects, search turn %s deg" % parts[3]
                else:
                    text = "Plan: " + " ".join(parts[1:])
                window.after(0, lambda t=text: show_plan(t))
                continue

            # Other status lines from auto.c
            if parts[0].upper() in ("ARRIVED", "STUCK", "BUMP"):
                window.after(0, lambda t="Plan: " + line: show_plan(t))
                continue

            # Scan data rows: angle raw smoothed
            if len(parts) == 3:
                try:
                    row = (float(parts[0]), float(parts[1]), float(parts[2]))
                except ValueError:
                    continue
                if not scanning:
                    begin_scan()        # no STARTSCAN seen, start one anyway
                scan_rows.append(row)
    except (OSError, ValueError):
        pass                                            # socket closed on quit

def start_connection():
    try:
        cybot_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        cybot_socket.connect((HOST, PORT))
    except OSError as e:
        set_label("Could not connect: " + str(e))
        return
    threading.Thread(target=sender_thread, args=(cybot_socket,), daemon=True).start()
    threading.Thread(target=receiver_thread, args=(cybot_socket,), daemon=True).start()

threading.Thread(target=start_connection, daemon=True).start()

# Keyboard shortcuts
window.bind("q", lambda event: buttonTurnLeft90())
window.bind("e", lambda event: buttonTurnRight90())
window.bind("t", lambda event: toggleMode())

window.mainloop()
