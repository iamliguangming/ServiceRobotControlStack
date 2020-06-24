#!/usr/bin/env python3
from PIL import Image
import numpy as np
import argparse
import matplotlib.pyplot as plt

def get_args():
    parser = argparse.ArgumentParser()
    parser.add_argument("occ_grid_path", help="Occupancy Grid Path")
    parser.add_argument("out_file", help="Output file")
    parser.add_argument("scale", help="Output file", type=float)
    args = parser.parse_args()
    return args.occ_grid_path, args.out_file, args.scale

map_path, out_path, scale = get_args()

drawn_lines = []

def draw_image():
    plt.imshow(plt.imread(map_path), cmap='bone')

def save_line(start_pt, end_pt):
    f = open(out_path, 'a')
    x1, y1 = start_pt
    x2, y2 = end_pt
    f.write("{}, {}, {}, {}\n".format(x1 * scale, y1 * scale, x2 * scale, y2 * scale))
    f.close()
    print("Added line")

def clear_drawn_lines():
    plt.gca().lines = []

def delete_last_line():
    f = open(out_path, 'r')
    ls = [l for l in f.readlines() if l != ""]
    f.close()
    if len(ls) > 0:
        f = open(out_path, 'w')
        f.writelines([l for l in ls[:-1]])
        f.close()



def plot_lines():
    global drawn_lines
    try:
        f = open(out_path, 'r')
        ls = f.readlines()
        colors = ['r', 'g', 'b', 'y']
        for idx, l in enumerate(ls):
            x1, y1, x2, y2 = [float(e) for e in l.split(', ')]
            l = plt.plot([x1 / scale, x2 / scale], [y1 / scale, y2 / scale], c=colors[idx % len(colors)])
            drawn_lines.append(l)
    except:
        pass



start_line = None
def onclick(event):
    global start_line
    if event.xdata != None and event.ydata != None and event.dblclick:
        print(event.xdata, event.ydata)
        if start_line is None:
            start_line = (event.xdata, event.ydata)
        else:
            save_line(start_line, (event.xdata, event.ydata))
            clear_drawn_lines()
            plot_lines()
            plt.gcf().canvas.draw()
            start_line = None

def press(event):
    global start_line
    if event.key == 'escape':
        start_line = None
    elif event.key == 'ctrl+z':
        clear_drawn_lines()
        delete_last_line()
        plot_lines()
        plt.gcf().canvas.draw()




draw_image()
plot_lines()
plt.gcf().canvas.mpl_connect('button_press_event', onclick)
plt.gcf().canvas.mpl_connect('key_press_event', press)
plt.show()