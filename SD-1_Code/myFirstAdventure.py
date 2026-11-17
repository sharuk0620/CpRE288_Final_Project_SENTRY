import tkinter as tk
window = tk.Tk()

label = tk.Label(
    text="Click the red button",
    fg="white",
    bg="black",
    width=30,
    height=10
)
label.pack()

entry = tk.Entry(fg="yellow", bg = "blue", width=50)
entry.pack()

def myClick():
    name=entry.get()
    myLabel1 = tk.Label(window, text="Look! " + name + " is not colorblind!!")
    myLabel1.pack()

def myOtherClick():
    name=entry.get()
    myLabel = tk.Label(window, text="Look! " + name + " is slightly colorblind!!")
    myLabel.pack()

button = tk.Button(
    text="Click me!",
    width = 25,
    height = 5,
    bg = "blue",
    fg="red",
    command=myOtherClick,
)
button.pack()

button = tk.Button(
    text="Click me!",
    width = 25,
    height = 5,
    bg = "red",
    fg="black",
    command=myClick,
)
button.pack()
