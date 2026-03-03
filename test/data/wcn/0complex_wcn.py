from pymol import cmd
from pymol.cgo import *
from pymol import cgo

cmd.load("0complex_wcn.pdb", "watpocket_input")

hull_obj = [
    cgo.LINEWIDTH, 4.0,
    cgo.BEGIN, cgo.LINES,
    cgo.COLOR, 0.20, 0.80, 1.00,
    cgo.VERTEX, 34.1530, 43.8980, 33.9190,
    cgo.VERTEX, 34.3470, 43.3230, 44.8910,
    cgo.VERTEX, 34.1530, 43.8980, 33.9190,
    cgo.VERTEX, 34.3130, 45.4960, 40.2400,
    cgo.VERTEX, 34.3130, 45.4960, 40.2400,
    cgo.VERTEX, 34.3470, 43.3230, 44.8910,
    cgo.VERTEX, 31.3150, 48.0610, 43.7750,
    cgo.VERTEX, 34.1530, 43.8980, 33.9190,
    cgo.VERTEX, 31.3150, 48.0610, 43.7750,
    cgo.VERTEX, 34.3130, 45.4960, 40.2400,
    cgo.VERTEX, 24.1010, 32.9380, 54.5390,
    cgo.VERTEX, 31.3150, 48.0610, 43.7750,
    cgo.VERTEX, 24.1010, 32.9380, 54.5390,
    cgo.VERTEX, 34.3470, 43.3230, 44.8910,
    cgo.VERTEX, 31.3150, 48.0610, 43.7750,
    cgo.VERTEX, 34.3470, 43.3230, 44.8910,
    cgo.VERTEX, 16.2010, 33.9270, 43.0710,
    cgo.VERTEX, 31.3150, 48.0610, 43.7750,
    cgo.VERTEX, 16.2010, 33.9270, 43.0710,
    cgo.VERTEX, 34.1530, 43.8980, 33.9190,
    cgo.VERTEX, 16.2010, 33.9270, 43.0710,
    cgo.VERTEX, 24.1010, 32.9380, 54.5390,
    cgo.VERTEX, 33.9050, 31.9180, 42.7570,
    cgo.VERTEX, 34.1530, 43.8980, 33.9190,
    cgo.VERTEX, 16.2010, 33.9270, 43.0710,
    cgo.VERTEX, 33.9050, 31.9180, 42.7570,
    cgo.VERTEX, 33.9050, 31.9180, 42.7570,
    cgo.VERTEX, 34.3470, 43.3230, 44.8910,
    cgo.VERTEX, 22.1610, 31.3230, 51.5610,
    cgo.VERTEX, 33.9050, 31.9180, 42.7570,
    cgo.VERTEX, 16.2010, 33.9270, 43.0710,
    cgo.VERTEX, 22.1610, 31.3230, 51.5610,
    cgo.VERTEX, 24.1010, 32.9380, 54.5390,
    cgo.VERTEX, 33.9050, 31.9180, 42.7570,
    cgo.VERTEX, 22.1610, 31.3230, 51.5610,
    cgo.VERTEX, 24.1010, 32.9380, 54.5390,
    cgo.END,
]

cmd.load_cgo(hull_obj, "watpocket_hull")
cmd.set("cgo_line_width", 4.0, "watpocket_hull")
cmd.show("cartoon", "watpocket_input")
cmd.select("watpocket", "watpocket_input and solvent and resi 570")
cmd.hide("everything", "solvent")
cmd.show("spheres", "watpocket")
cmd.set("sphere_scale", 0.4, "watpocket")
cmd.orient("watpocket_hull")
print("watpocket: loaded hull with 18 edges")
