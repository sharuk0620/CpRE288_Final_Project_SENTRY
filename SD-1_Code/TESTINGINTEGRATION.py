import tkinter as tk
import threading
import queue
import socket
import os
import numpy as np
import matplotlib
matplotlib.use("TkAgg")
from matplotlib.figure import Figure
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg

# ---------- Settings ----------
HOST = "192.168.1.1"   # same IP you use in PuTTY
PORT = 288             # same port you use in PuTTY

# What each button sends to the CyBot (CHANGE these to match your main.c!)
CMD_FORWARD       = "w\n"
CMD_BACKWARD      = "s\n"
CMD_LEFT          = "a\n"
CMD_RIGHT         = "d\n"
CMD_SCAN          = "m\n"   # "m" is what your client file used for scan
CMD_TURN_LEFT_90  = "q\n"   # Turn 90 Left
CMD_TURN_RIGHT_90 = "e\n"   # Turn 90 Right
CMD_QUIT          = "quit\n"

filename = "sensor-scan.txt"
full_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), filename)

# Colors
BG = "black"
FG = "lime"      # green for the plot (try "#00ff00" or "limegreen" for other shades)

# GUI thread puts commands here, socket thread takes them out
command_queue = queue.Queue()

window = tk.Tk()
window.title("CyBot Control")
window.configure(bg=BG)

# ---------- Label ----------
message_label = tk.Label(window, text="", font=("Times New Roman", 12), bg=BG, fg=FG)
message_label.grid(row=3, column=1)

# ---------- Button actions (update label AND send command) ----------
def buttonForward():
    message_label.config(text="MOVING FORWARD")
    command_queue.put(CMD_FORWARD)

def buttonBackward():
    message_label.config(text="MOVING BACKWARD")
    command_queue.put(CMD_BACKWARD)

def buttonLeft():
    message_label.config(text="MOVING LEFTWARD")
    command_queue.put(CMD_LEFT)

def buttonRight():
    message_label.config(text="MOVING RIGHTWARD")
    command_queue.put(CMD_RIGHT)

def buttonScan():
    message_label.config(text="SCANNING")
    command_queue.put(CMD_SCAN)

def buttonTurnLeft90():
    message_label.config(text="TURNING 90 LEFT")
    command_queue.put(CMD_TURN_LEFT_90)

def buttonTurnRight90():
    message_label.config(text="TURNING 90 RIGHT")
    command_queue.put(CMD_TURN_RIGHT_90)

def on_close():
    command_queue.put(CMD_QUIT)
    window.after(500, window.destroy)   # give the thread a moment to send quit

window.protocol("WM_DELETE_WINDOW", on_close)

# ---------- Buttons ----------
# Main buttons: black with red text
btn_style = dict(width=25, height=5, bg="black", fg="red",
                 activebackground="#222222", activeforeground="red")

# Accessory buttons: dark gray, smaller
accessory_style = dict(width=16, height=2, bg="#3a3a3a", fg="red",
                       activebackground="#555555", activeforeground="red",
                       font=("Arial", 9))

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

btn_backward = tk.Button(window, text="BACKWARD", command=buttonBackward, **btn_style)
btn_backward.grid(row=2, column=1)

# (row 2, column 2 is intentionally left empty)

# ---------- Polar plot embedded in the window ----------
fig = Figure(figsize=(5, 4), facecolor=BG)
ax = fig.add_subplot(111, projection="polar")
canvas = FigureCanvasTkAgg(fig, master=window)
canvas.get_tk_widget().configure(bg=BG, highlightthickness=0)
canvas.get_tk_widget().grid(row=4, column=0, columnspan=3)

# ---------- Objects list under the plot ----------
objects_label = tk.Label(window, text="Objects detected: (no scan yet)",
                         font=("Courier New", 12), bg=BG, fg=FG, justify="left")
objects_label.grid(row=5, column=0, columnspan=3, pady=(5, 10))

def show_objects(objects):
    """Update the text under the plot (runs in the GUI thread)."""
    if not objects:
        objects_label.config(text="Objects detected: 0")
        return
    lines = ["Objects detected: " + str(len(objects))]
    for i, (angle, dist, width) in enumerate(objects, start=1):
        lines.append("Object %d:  angle %.1f deg   distance %.2f cm   width %.2f cm"
                     % (i, angle, dist, width))
    objects_label.config(text="\n".join(lines))

def setup_axes():
    """0-180 degree half-circle setup, styled green on black."""
    ax.set_facecolor(BG)                          # plot area background
    ax.set_thetamin(0)
    ax.set_thetamax(180)
    ax.set_rmax(100)
    ax.set_rticks([20, 40, 60, 80, 100])
    ax.set_rlabel_position(-22.5)
    ax.set_xticks(np.arange(0, np.pi + .1, np.pi / 4))
    ax.tick_params(axis='both', which='major', labelsize=14, colors=FG)   # tick labels
    ax.grid(True, color=FG, alpha=0.5)            # green grid lines
    for spine in ax.spines.values():              # outer edge of the plot
        spine.set_edgecolor(FG)
    ax.set_title("CyBot Sensor Scan from 0 to 180 Degrees", size=14, y=1.0, pad=-24, color=FG)

setup_axes()      # draw the empty 180 plot right away
canvas.draw()

def plot_scan():
    """Read sensor-scan.txt and draw the polar plot (runs in the GUI thread)."""
    angle_degrees = []
    distance = []
    with open(full_path, "r") as f:
        f.readline()                       # skip header row
        for line in f:
            data = line.split()
            if len(data) < 3:
                continue
            angle_degrees.append(float(data[0]))   # column 0 = angle
            distance.append(float(data[2]))        # column 2 = smoothed distance (cm)

    angle_radians = (np.pi / 180) * np.array(angle_degrees)

    ax.clear()                             # clear() resets everything...
    ax.plot(angle_radians, distance, color=FG, linewidth=4.0)   # scan line
    setup_axes()                           # ...so re-apply the 180 setup and colors
    canvas.draw()

# ---------- Socket thread ----------
def set_label(text):
    window.after(0, lambda: message_label.config(text=text))

def socket_thread():
    try:
        cybot_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        cybot_socket.connect((HOST, PORT))
    except OSError as e:
        set_label("Could not connect: " + str(e))
        return

    cybot = cybot_socket.makefile("rbw", buffering=0)

    while True:
        send_message = command_queue.get()          # waits here until a button is pressed
        cybot.write(send_message.encode())
        print("Sent to server: " + send_message)

        if send_message == CMD_QUIT:
            break

        if send_message == CMD_SCAN:
            file_object = open(full_path, "w")
            file_object.write("Angle Raw Smoothed\n")   # header row for plot_scan to skip
            objects = []                               # (angle, distance, width) for each object

            while True:
                rx_message = cybot.readline().decode()
                if rx_message == "":                   # connection closed
                    break

                line = rx_message.strip()
                print(line)

                # Stop when the CyBot says the scan is finished
                if "SCAN DONE" in line.upper():
                    break

                parts = line.split()

                # Object lines: OBJECT angle distance width
                if len(parts) >= 4 and parts[0].upper() == "OBJECT":
                    try:
                        objects.append((float(parts[1]), float(parts[2]), float(parts[3])))
                    except ValueError:
                        pass
                    continue

                # Scan data lines: angle raw smoothed
                if len(parts) >= 3:
                    try:
                        float(parts[0]); float(parts[1]); float(parts[2])
                        file_object.write(parts[0] + " " + parts[1] + " " + parts[2] + "\n")
                    except ValueError:
                        pass

            file_object.close()

            window.after(0, plot_scan)                          # draw the plot in the GUI thread
            window.after(0, lambda objs=objects: show_objects(objs))   # list objects under the plot
            set_label("SCAN COMPLETE")
        else:
            rx_message = cybot.readline()           # wait for CyBot's reply
            print("Got a message from server: " + rx_message.decode())

    cybot.close()
    cybot_socket.close()

threading.Thread(target=socket_thread, daemon=True).start()

# Keyboard shortcuts for the turn buttons
window.bind("q", lambda event: buttonTurnLeft90())
window.bind("e", lambda event: buttonTurnRight90())

window.mainloop()
