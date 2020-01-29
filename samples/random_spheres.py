#!/usr/bin/env python
import sys, os
scriptdir = os.path.split(__file__)[0]
sys.path.insert(0, os.path.join(scriptdir, '..'))

import numpy
from PIL import Image
import ospray

W = 1024
H = 1024
S = 8
RENDERER = 'scivis'

cam_pos = (0.5, 0.5, 3.0)
cam_up = (0.0, 1.0, 0.0)
cam_view = (0.0, 0.0, -1.0)

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
device.commit()

N = 50000
if len(argv) > 1:
    N = int(argv[1])
    if len(argv) >= 3:
        W = int(argv[2])
        H = int(argv[3])

camera = ospray.Camera('perspective')
camera.set_param('aspect', W/H)
camera.set_param('position', cam_pos)
camera.set_param('direction', cam_view)
camera.set_param('up', cam_up)
camera.set_param('fovy', 32.0)
camera.commit()

max_radius = 0.7 / pow(N, 1/3)

numpy.random.seed(123456)
positions = numpy.random.rand(N,3).astype(numpy.float32)
radii = max_radius*numpy.random.rand(N).astype(numpy.float32)
colors = 0.3 + 0.7*numpy.random.rand(N,3).astype(numpy.float32)
opacities = numpy.ones(N, 'float32')
colors = numpy.c_[ colors, opacities ]
assert(numpy.min(colors) >= 0.3)

#colors = numpy.tile(numpy.array([1,0,0,1],'float32'), (N,1))

spheres = ospray.Geometry('sphere')
spheres.set_param('sphere.position', ospray.data_constructor_vec(positions, True))
spheres.set_param('sphere.radius', ospray.data_constructor(radii, True))
spheres.commit()

gmodel = ospray.GeometricModel(spheres)
# XXX sometimes segfault, plus some spheres are black which should not be possible given
# the colors set
gmodel.set_param('color', ospray.data_constructor_vec(colors))
gmodel.commit()

group = ospray.Group()
group.set_param('geometry', [gmodel])
group.commit()

instance = ospray.Instance(group)
instance.commit()

if RENDERER == 'pathtracer':
    material = ospray.Material(RENDERER, 'principled')
    material.set_param('metallic', 0.)
    material.commit()
else:
    material = ospray.Material(RENDERER, 'obj')
    material.commit()
    
gmodel.set_param('material', material)
gmodel.commit()

world = ospray.World()
world.set_param('instance', [instance])

light1 = ospray.Light('ambient')
light1.set_param('color', (1.0, 1.0, 1.0))
light1.set_param('intensity', 0.8)
light1.commit()

light2 = ospray.Light('distant')
light2.set_param('direction', (-1.0, -1.0, -1.0))
light2.set_param('intensity', 0.8)
light2.commit()

lights = [light1, light2]

world.set_param('light', lights)
world.commit()
#print(world.get_bounds())

renderer = ospray.Renderer(RENDERER)
renderer.set_param('backgroundColor', (1.0, 1.0, 1.0, 1.0))
renderer.set_param('maxPathLength', 1000)
renderer.commit()

format = ospray.OSP_FB_SRGBA
channels = int(ospray.OSP_FB_COLOR) | int(ospray.OSP_FB_ACCUM) 

framebuffer = ospray.FrameBuffer((W,H), format, channels)
framebuffer.clear()

for frame in range(S):
    future = framebuffer.render_frame(renderer, camera, world)
    future.wait()
    sys.stdout.write('.')
    sys.stdout.flush()
    
sys.stdout.write('\n')

colors = framebuffer.get(ospray.OSP_FB_COLOR, (W,H), format)

img = Image.frombuffer('RGBA', (W,H), colors, 'raw', 'RGBA', 0, 1)
img = img.transpose(Image.FLIP_TOP_BOTTOM)
img.save('spheres.png')
