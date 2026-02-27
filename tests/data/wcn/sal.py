from pymol import cmd, cgo

cmd.load("complex_wcn.pdb", "watpocket_input")

hull_obj = [
    cgo.LINEWIDTH, 4.0,
    cgo.BEGIN, cgo.LINES,
    cgo.COLOR, 0.20, 0.80, 1.00,
    cgo.VERTEX, 17.0320, 36.7770, 43.8320,
    cgo.VERTEX, 22.6270, 34.4710, 50.7790,
    cgo.VERTEX, 17.0320, 36.7770, 43.8320,
    cgo.VERTEX, 24.9250, 34.9250, 53.7840,
    cgo.VERTEX, 17.0320, 36.7770, 43.8320,
    cgo.VERTEX, 33.6280, 49.1690, 42.5240,
    cgo.VERTEX, 17.0320, 36.7770, 43.8320,
    cgo.VERTEX, 33.7850, 33.3970, 41.2550,
    cgo.VERTEX, 17.0320, 36.7770, 43.8320,
    cgo.VERTEX, 34.9800, 44.9640, 32.3020,
    cgo.VERTEX, 22.6270, 34.4710, 50.7790,
    cgo.VERTEX, 24.9250, 34.9250, 53.7840,
    cgo.VERTEX, 22.6270, 34.4710, 50.7790,
    cgo.VERTEX, 33.7850, 33.3970, 41.2550,
    cgo.VERTEX, 24.9250, 34.9250, 53.7840,
    cgo.VERTEX, 33.6280, 49.1690, 42.5240,
    cgo.VERTEX, 24.9250, 34.9250, 53.7840,
    cgo.VERTEX, 33.7850, 33.3970, 41.2550,
    cgo.VERTEX, 24.9250, 34.9250, 53.7840,
    cgo.VERTEX, 36.4310, 44.4300, 43.0000,
    cgo.VERTEX, 33.6280, 49.1690, 42.5240,
    cgo.VERTEX, 34.9800, 44.9640, 32.3020,
    cgo.VERTEX, 33.6280, 49.1690, 42.5240,
    cgo.VERTEX, 35.8940, 46.5850, 38.6760,
    cgo.VERTEX, 33.6280, 49.1690, 42.5240,
    cgo.VERTEX, 36.4310, 44.4300, 43.0000,
    cgo.VERTEX, 33.7850, 33.3970, 41.2550,
    cgo.VERTEX, 34.9800, 44.9640, 32.3020,
    cgo.VERTEX, 33.7850, 33.3970, 41.2550,
    cgo.VERTEX, 36.4310, 44.4300, 43.0000,
    cgo.VERTEX, 34.9800, 44.9640, 32.3020,
    cgo.VERTEX, 35.8940, 46.5850, 38.6760,
    cgo.VERTEX, 34.9800, 44.9640, 32.3020,
    cgo.VERTEX, 36.4310, 44.4300, 43.0000,
    cgo.VERTEX, 35.8940, 46.5850, 38.6760,
    cgo.VERTEX, 36.4310, 44.4300, 43.0000,
    cgo.END,
]

cmd.load_cgo(hull_obj, "watpocket_hull")
cmd.set("cgo_line_width", 4.0, "watpocket_hull")
cmd.show("cartoon", "watpocket_input")
cmd.select("watpocket", "watpocket_input and solvent and resi 3307+3416+4776+5191+6324")
cmd.hide("everything", "solvent")
cmd.show("spheres", "watpocket")
cmd.set("sphere_scale", 0.4, "watpocket")
cmd.orient("watpocket_hull")
print("watpocket: loaded hull with 18 edges")
