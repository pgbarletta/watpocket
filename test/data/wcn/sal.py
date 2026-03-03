from pymol import cmd
from pymol.cgo import *
from pymol import cgo

cmd.load("1complex_wcn.pdb", "watpocket_input")

hull_obj = [
    cgo.LINEWIDTH, 4.0,
    cgo.BEGIN, cgo.LINES,
    cgo.COLOR, 0.20, 0.80, 1.00,
    cgo.VERTEX, 33.8490, 28.2430, 34.3960,
    cgo.VERTEX, 34.9520, 39.2320, 35.4740,
    cgo.VERTEX, 33.8490, 28.2430, 34.3960,
    cgo.VERTEX, 34.8480, 41.0120, 30.6580,
    cgo.VERTEX, 34.8480, 41.0120, 30.6580,
    cgo.VERTEX, 34.9520, 39.2320, 35.4740,
    cgo.VERTEX, 32.2480, 43.8800, 34.4830,
    cgo.VERTEX, 32.9260, 38.8560, 24.9000,
    cgo.VERTEX, 32.2480, 43.8800, 34.4830,
    cgo.VERTEX, 34.8480, 41.0120, 30.6580,
    cgo.VERTEX, 32.9260, 38.8560, 24.9000,
    cgo.VERTEX, 34.8480, 41.0120, 30.6580,
    cgo.VERTEX, 26.9740, 29.9240, 47.4960,
    cgo.VERTEX, 32.2480, 43.8800, 34.4830,
    cgo.VERTEX, 26.9740, 29.9240, 47.4960,
    cgo.VERTEX, 34.9520, 39.2320, 35.4740,
    cgo.VERTEX, 32.2480, 43.8800, 34.4830,
    cgo.VERTEX, 34.9520, 39.2320, 35.4740,
    cgo.VERTEX, 15.0590, 30.8490, 37.3100,
    cgo.VERTEX, 32.2480, 43.8800, 34.4830,
    cgo.VERTEX, 15.0590, 30.8490, 37.3100,
    cgo.VERTEX, 32.9260, 38.8560, 24.9000,
    cgo.VERTEX, 32.9260, 38.8560, 24.9000,
    cgo.VERTEX, 33.8490, 28.2430, 34.3960,
    cgo.VERTEX, 15.0590, 30.8490, 37.3100,
    cgo.VERTEX, 33.8490, 28.2430, 34.3960,
    cgo.VERTEX, 15.0590, 30.8490, 37.3100,
    cgo.VERTEX, 26.9740, 29.9240, 47.4960,
    cgo.VERTEX, 24.3760, 28.5710, 45.0820,
    cgo.VERTEX, 33.8490, 28.2430, 34.3960,
    cgo.VERTEX, 15.0590, 30.8490, 37.3100,
    cgo.VERTEX, 24.3760, 28.5710, 45.0820,
    cgo.VERTEX, 26.9740, 29.9240, 47.4960,
    cgo.VERTEX, 33.8490, 28.2430, 34.3960,
    cgo.VERTEX, 24.3760, 28.5710, 45.0820,
    cgo.VERTEX, 26.9740, 29.9240, 47.4960,
    cgo.END,
]

cmd.load_cgo(hull_obj, "watpocket_hull")
cmd.set("cgo_line_width", 4.0, "watpocket_hull")
cmd.show("cartoon", "watpocket_input")
cmd.select("watpocket", "watpocket_input and solvent and resi 864+1348+1498+1499+1611+1738+1907+2706+3136+3173+4102+4513+4545+4843+4984+5835+5897+6312")
cmd.hide("everything", "solvent")
cmd.show("spheres", "watpocket")
cmd.set("sphere_scale", 0.4, "watpocket")
cmd.orient("watpocket_hull")
print("watpocket: loaded hull with 18 edges")
