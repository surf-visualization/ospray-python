#!/usr/bin/env python
import sys, getopt, math, os
scriptdir = os.path.split(__file__)[0]
sys.path.insert(0, os.path.join(scriptdir, '..'))

import numpy
from PIL import Image
import ospray
from loaders import read_ply, read_obj, read_stl, read_pdb

W = 1024
H = 768
FOVY = 45.0

argv = ospray.init(sys.argv)

# Enable logging output

def error_callback(error, details):
    print('OSPRAY ERROR: %d (%s)' % (error, details))
    
def status_callback(message):
    print('OSPRAY STATUS: %s' % message)
    
ospray.set_error_callback(error_callback)
ospray.set_status_callback(status_callback)

device = ospray.get_current_device()
device.set_param('logLevel', 1)
#device.set_param('logOutput', 'cerr')
#device.set_param('errorOutput', 'cerr')
device.commit()

# Parse arguments

force_subdivision_mesh = False
renderer_type = 'pathtracer'
debug_renderer_type = 'primID'
samples = 8
subdivision_level = 5.0
display_result = False

def usage():
    print('Usage: %s [options] file.ply|file.obj|file.stl|file.pdb' % sys.argv[0])
    print()
    print('-d type          Use debug renderer of specific type')
    print('-l subdivlevel   Subdivision level (default: %f)' % subdivision_level)
    print('-s               Force subdivision mesh')
    print('-x               Display result (requires tkinter module)')
    print('-h               Help')
    print()

optlist, args = getopt.getopt(argv[1:], 'd:hl:sx')

for o, a in optlist:
    if o == '-d':
        renderer_type = 'debug'
        debug_renderer_type = a
        if debug_renderer_type not in ['rayDir','eyeLight','primID','geomID','instID','Ng','Ns','backfacing_Ng','backfacing_Ns','dPds','dPdt','volume']:
            print('Debug renderer will show test frame')
    elif o == '-h':
        usage()
        sys.exit(-1)
    elif o == '-l':
        subdivision_level = float(a)
    elif o == '-s':
        force_subdivision_mesh = True
    elif o == '-x':
        display_result = True

if len(args) != 1:
    usage()
    sys.exit(-1)

# Set up material

material = None
if renderer_type in ['scivis', 'pathtracer']:
    if False:
        # Gold
        material = ospray.Material(renderer_type, 'metal')
        material.set_param('eta', (0.07, 0.37, 1.5))
        material.set_param('k', (3.7, 2.3, 1.7))
        material.set_param('roughness', 0.5)
    else:
        material = ospray.Material(renderer_type, 'obj')
        #material.set_param('Kd', (0, 0, 1.0))
        #material.set_param('Ns', 1.0)

    material.commit()

# Process file

fname = args[0]
ext = os.path.splitext(fname)[-1]

if ext == '.ply':
    meshes = read_ply(fname, force_subdivision_mesh)
elif ext in ['.obj', '.OBJ']:
    meshes = read_obj(fname, force_subdivision_mesh)
elif ext == '.stl':
    meshes = read_stl(fname, force_subdivision_mesh)
elif ext == '.pdb':
    meshes = read_pdb(fname)
else:
    raise ValueError('Unknown extension %s' % ext)

if isinstance(meshes, ospray.GeometricModel):
    meshes.set_param('material', material)
    meshes.commit()    
    gmodels = [meshes]
else:
    gmodels = []
    for mesh in meshes:
        
        if force_subdivision_mesh:
            mesh.set_param('level', subdivision_level)
        
        gmodel = ospray.GeometricModel(mesh)
        if material is not None:
            gmodel.set_param('material', material)
        gmodel.commit()
    
        gmodels.append(gmodel)
    
if len(gmodels) == 0:
    print('No models to render!')
    sys.exit(-1)
    
print('Have %d meshes' % len(gmodels))

group = ospray.Group()
group.set_param('geometry', gmodels)
group.commit()

instance1 = ospray.Instance(group)
instance1.set_param('transform', ospray.mat4.identity())
instance1.commit()

instances = [instance1]

if False:
    instance2 = ospray.Instance(group)
    instance2.set_param('transform', ospray.mat4.translate(1, 0, 0))
    instance2.commit()

    instance3 = ospray.Instance(group)
    instance3.set_param('transform', 
        ospray.mat4.translate(0, 1, 0) * ospray.mat4.rotate(90, 0, 0, 1)
        )
    instance3.commit()

    instances = [instance1, instance2, instance3]

world = ospray.World()
world.set_param('instance', instances)

light1 = ospray.Light('ambient')
light1.set_param('color', (1.0, 1.0, 1.0))
light1.set_param('intensity', 0.4)
light1.commit()

light2 = ospray.Light('distant')
light2.set_param('intensity', 0.6)
light2.commit()

lights = [light1, light2]

world.set_param('light', lights)
world.commit()
print('World bound', world.get_bounds())

# Camera
bound = world.get_bounds()
bound = numpy.array(bound, dtype=numpy.float32).reshape((-1, 3))
print('BOUND', bound[0], bound[1])

diagonal = numpy.linalg.norm(bound[1] - bound[0])
radius = 0.5*diagonal
distance = radius / math.tan(0.5*math.radians(FOVY)) * 1.05
print('diagonal', diagonal)
print('radius', radius)
print('distance', distance)

center = 0.5*(bound[0] + bound[1])
position = center - distance/math.sqrt(3)*numpy.array([-1,-1,-1],'float32')

cam_pos = tuple(position.tolist())
cam_up = (0.0, 0, 1)
cam_view = tuple((center - position).tolist())

print('pos', cam_pos, 'cam_view', cam_view, 'cam_up', cam_up)

camera = ospray.Camera('perspective')
camera.set_param('aspect', W/H)
camera.set_param('position', cam_pos)
camera.set_param('direction', cam_view)
camera.set_param('up', cam_up)
camera.set_param('fovy', FOVY)
camera.commit()

light2.set_param('direction', cam_view)
light2.commit()

# Render
renderer = ospray.Renderer(renderer_type)
renderer.set_param('backgroundColor', (1.0, 1, 1, 1))
if renderer_type == 'debug':
    renderer.set_param('method', debug_renderer_type)
renderer.commit()

format = ospray.OSP_FB_SRGBA
channels = int(ospray.OSP_FB_COLOR) | int(ospray.OSP_FB_ACCUM) | int(ospray.OSP_FB_DEPTH) | int(ospray.OSP_FB_VARIANCE) | int(ospray.OSP_FB_NORMAL) | int(ospray.OSP_FB_ALBEDO)

framebuffer = ospray.FrameBuffer(W, H, format, channels)
framebuffer.clear()

for frame in range(samples):
    future = framebuffer.render_frame(renderer, camera, world)
    future.wait()
    sys.stdout.write('.')
    sys.stdout.flush()
    
sys.stdout.write('\n')

colors = framebuffer.get(ospray.OSP_FB_COLOR, (W,H), format)
print(colors.shape)
img = Image.frombuffer('RGBA', (W,H), colors, 'raw', 'RGBA', 0, 1)
img = img.transpose(Image.FLIP_TOP_BOTTOM)
img.save('colors.png')

if False:
    depth = framebuffer.get(ospray.OSP_FB_DEPTH, (W,H))
    print(depth.shape)
    print(numpy.min(depth), numpy.max(depth))
    depth[depth == numpy.inf] = 1000.0
    img = Image.frombuffer('F', (W,H), depth, 'raw', 'F', 0, 1)
    img = img.transpose(Image.FLIP_TOP_BOTTOM)
    img.save('depth.tif')

if False:
    # XXX not working yet
    normal = framebuffer.get(ospray.OSP_FB_NORMAL, (W,H))
    print(normal.shape)
    print(numpy.min(normal), numpy.max(normal))
    normal = ((0.5 + 0.5*normal)*255).astype(numpy.uint8)
    img = Image.frombuffer('RGB', (W,H), depth, 'raw', 'RGB', 0, 1)
    img = img.transpose(Image.FLIP_TOP_BOTTOM)
    img.save('normal.png')

if False:
    # XXX not working yet
    albedo = framebuffer.get(ospray.OSP_FB_ALBEDO, (W,H))
    print(albedo.shape)
    print(numpy.min(albedo), numpy.max(albedo))
    albedo = ((0.5 + 0.5*albedo)*255).astype(numpy.uint8)
    img = Image.frombuffer('RGB', (W,H), depth, 'raw', 'RGB', 0, 1)
    img = img.transpose(Image.FLIP_TOP_BOTTOM)
    img.save('albedo.png')

# Will raise exception as OSP_FB_ACCUM cannot be mapped
#framebuffer.get(ospray.OSP_FB_ACCUM, (W,H))

if display_result:
    from PIL import ImageTk
    from tkinter import *
    
    # https://stackoverflow.com/a/16536496
    root = Tk()
    tkimg = ImageTk.PhotoImage(img)
    panel = Label(root, image = tkimg)
    panel.pack(side = "bottom", fill = "both", expand = "yes")
    
    root.bind("<Escape>", lambda e: sys.exit(-1))
    root.mainloop()