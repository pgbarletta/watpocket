from pymol import cmd, cgo

cmd.load("complex_wcn.pdb", "watpocket_input")

hull_obj = [
    cgo.LINEWIDTH, 4.0,
    cgo.BEGIN, cgo.LINES,
    cgo.COLOR, 0.20, 0.80, 1.00,
    cgo.VERTEX, 19.7110, 37.5290, 41.4980,
    cgo.VERTEX, 25.3570, 35.8620, 53.8240,
    cgo.VERTEX, 19.7110, 37.5290, 41.4980,
    cgo.VERTEX, 25.7790, 39.1260, 51.7470,
    cgo.VERTEX, 19.7110, 37.5290, 41.4980,
    cgo.VERTEX, 30.6210, 33.1320, 38.8040,
    cgo.VERTEX, 19.7110, 37.5290, 41.4980,
    cgo.VERTEX, 34.0370, 51.5770, 39.8790,
    cgo.VERTEX, 19.7110, 37.5290, 41.4980,
    cgo.VERTEX, 34.3080, 47.7200, 29.8170,
    cgo.VERTEX, 25.3570, 35.8620, 53.8240,
    cgo.VERTEX, 25.7790, 39.1260, 51.7470,
    cgo.VERTEX, 25.3570, 35.8620, 53.8240,
    cgo.VERTEX, 30.6210, 33.1320, 38.8040,
    cgo.VERTEX, 25.3570, 35.8620, 53.8240,
    cgo.VERTEX, 34.2610, 42.6450, 46.2140,
    cgo.VERTEX, 25.7790, 39.1260, 51.7470,
    cgo.VERTEX, 34.0370, 51.5770, 39.8790,
    cgo.VERTEX, 25.7790, 39.1260, 51.7470,
    cgo.VERTEX, 34.2610, 42.6450, 46.2140,
    cgo.VERTEX, 30.6210, 33.1320, 38.8040,
    cgo.VERTEX, 34.2610, 42.6450, 46.2140,
    cgo.VERTEX, 30.6210, 33.1320, 38.8040,
    cgo.VERTEX, 34.3080, 47.7200, 29.8170,
    cgo.VERTEX, 34.0370, 51.5770, 39.8790,
    cgo.VERTEX, 34.2610, 42.6450, 46.2140,
    cgo.VERTEX, 34.0370, 51.5770, 39.8790,
    cgo.VERTEX, 34.3080, 47.7200, 29.8170,
    cgo.VERTEX, 34.2610, 42.6450, 46.2140,
    cgo.VERTEX, 34.3080, 47.7200, 29.8170,
    cgo.END,
]

cmd.load_cgo(hull_obj, "watpocket_hull")
cmd.set("cgo_line_width", 4.0, "watpocket_hull")
cmd.show("cartoon", "watpocket_input")
cmd.select("watpocket", "watpocket_input and solvent and resi 915+1512+2055+3322+4781+4900+5108")
cmd.hide("everything", "solvent")
cmd.show("spheres", "watpocket")
cmd.set("sphere_scale", 0.4, "watpocket")
cmd.orient("watpocket_hull")
print("watpocket: loaded hull with 15 edges")
