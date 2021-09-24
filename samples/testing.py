#!/usr/bin/env python
import sys, os
scriptdir = os.path.split(__file__)[0]
sys.path.insert(0, os.path.join(scriptdir, '..'))

import numpy
from PIL import Image
import ospray

W = 1024
H = 768
#RENDERER = 'scivis'
RENDERER = 'pathtracer'
samples = 4

args = ospray.init(sys.argv)

"""
static const std::vector<std::string> g_scenes = {"boxes",
                                                  "cornell_box",
                                                  "curves",
                                                  "gravity_spheres_volume",
                                                  "gravity_spheres_isosurface",
                                                  "perlin_noise_volumes",
                                                  "random_spheres",
                                                  "streamlines",
                                                  "subdivision_cube",
                                                  "unstructured_volume"};
"""

SCENE = 'boxes'
if len(args) > 1:
    SCENE = args[1]

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

# Create scene
builder = ospray.testing.new_builder(SCENE)
builder.set_param('rendererType', RENDERER)
builder.commit()

if True:
    world = builder.build_world()
    world.commit()
else:
    group = scene.build_group()
    group.commit()

    instance = ospray.Instance(group)
    instance.commit()

    world = ospray.World()
    world.set_param('instance', [instance])
    world.commit()

# Set up the rest

bound = world.get_bounds()
bound = numpy.array(bound, dtype=numpy.float32).reshape((-1, 3))
print(bound)

center = 0.5*(bound[0] + bound[1])
print(center)
    
position = center + 3*(bound[1] - center)

cam_pos = tuple(position.tolist() + numpy.array((1,1,1),'float32'))
cam_up = (0.0, 1, 0)
cam_view = tuple((center - position + numpy.array((0,0,2),'float32')).tolist())

camera = ospray.Camera('perspective')
camera.set_param('aspect', W/H)
camera.set_param('position', cam_pos)
camera.set_param('direction', cam_view)
camera.set_param('up', cam_up)
camera.set_param('fovy', 55.0)
camera.commit()

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

renderer = ospray.Renderer(RENDERER)
renderer.set_param('backgroundColor', (1.0, 1.0, 1.0, 1.0))
renderer.commit()

format = ospray.OSP_FB_SRGBA
channels = int(ospray.OSP_FB_COLOR) | int(ospray.OSP_FB_ACCUM) | int(ospray.OSP_FB_DEPTH) | int(ospray.OSP_FB_VARIANCE)

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
img.save('testing.png')

