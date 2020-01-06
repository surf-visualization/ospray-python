#!/usr/bin/env python
import sys, getopt, os
scriptdir = os.path.split(__file__)[0]
sys.path.insert(0, os.path.join(scriptdir, '..'))

import numpy
from PIL import Image
import ospray
from loaders import read_ply, read_obj, read_stl

W = 1024
H = 768

argv = ospray.init(sys.argv)

# Enable logging output

def error_callback(error, details):
    print('OSPRAY ERROR: %d (%s)' % (error, details))
    
def status_callback(message):
    print('OSPRAY STATUS: %s' % message)
    
ospray.set_error_func(error_callback)
ospray.set_status_func(status_callback)

device = ospray.get_current_device()
device.set('logLevel', 1)
#device.set('logOutput', 'cerr')
#device.set('errorOutput', 'cerr')
device.commit()

# Parse arguments

force_subdivision_mesh = False
samples = 8
subvision_level = 5.0

optlist, args = getopt.getopt(argv[1:], 'l:s')

for o, a in optlist:
    if o == '-l':
        subvision_level = float(a)
    elif o == '-s':
        force_subdivision_mesh = True

# Set up material

if False:
    # Gold
    material = ospray.Material('pathtracer', 'Metal')
    material.set_param('eta', (0.07, 0.37, 1.5))
    material.set_param('k', (3.7, 2.3, 1.7))
    material.set_param('roughness', 0.5)
else:
    material = ospray.Material('pathtracer', 'OBJMaterial')
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
else:
    raise ValueError('Unknown extension %s' % ext)

gmodels = []
for mesh in meshes:
    
    if force_subdivision_mesh:
        mesh.set_param('level', subvision_level)
    
    gmodel = ospray.GeometricModel(mesh)
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

bound = group.get_bounds()
bound = numpy.array(bound, dtype=numpy.float32).reshape((-1, 3))
print(bound)

center = 0.5*(bound[0] + bound[1])
print(center)
    
position = center + 3*(bound[1] - center)

cam_pos = tuple(position.tolist())
cam_up = (0.0, 0, 1)
cam_view = tuple((center - position).tolist())

camera = ospray.Camera('perspective')
camera.set_param('aspect', W/H)
camera.set_param('position', cam_pos)
camera.set_param('direction', cam_view)
camera.set_param('up', cam_up)
camera.commit()

instance1 = ospray.Instance(group)
instance1.set_param('xfm', ospray.affine3f.identity())
instance1.commit()

instances = [instance1]

if False:
    instance2 = ospray.Instance(group)
    instance2.set_param('xfm', ospray.affine3f.translate((1,0,0)))
    instance2.commit()

    instance3 = ospray.Instance(group)
    instance3.set_param('xfm', 
        ospray.affine3f.translate((0,1,0)) * ospray.affine3f.rotate((0,0,1), 90)
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
light2.set_param('direction', cam_view)
light2.set_param('intensity', 0.6)
light2.commit()

lights = [light1, light2]

world.set_param('light', lights)
world.commit()
print('World bound', world.get_bounds())

renderer = ospray.Renderer('pathtracer')
renderer.set_param('bgColor', (1.0, 1, 1, 0))
renderer.commit()

format = ospray.OSP_FB_SRGBA
channels = int(ospray.OSP_FB_COLOR) | int(ospray.OSP_FB_ACCUM) | int(ospray.OSP_FB_DEPTH) | int(ospray.OSP_FB_VARIANCE) | int(ospray.OSP_FB_NORMAL) | int(ospray.OSP_FB_ALBEDO)

framebuffer = ospray.FrameBuffer((W,H), format, channels)
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