#!/usr/bin/env python
import sys, os
scriptdir = os.path.split(__file__)[0]
sys.path.insert(0, os.path.join(scriptdir, '..'))

import numpy
from PIL import Image
import ospray

W = 1024
H = 768

cam_pos = (1.0, 1.0, 3.0)
cam_up = (0.0, 1.0, 0.0)
look_at = numpy.array([0, 0, 0], numpy.float32)
cam_view = tuple((look_at - numpy.array(cam_pos, numpy.float32)).tolist())

vertex = numpy.array([
   [-1.0, -1.0, 0.0],
   [-1.0, 1.0, 0.0],
   [1.0, -1.0, 0.0],
   [1.0, 1.0, 0.0]
], dtype=numpy.float32)

texcoord = numpy.array([
    [0.0, 0.0],
    [0.0, 1.0],
    [1.0, 0.0],
    [1.0, 1.0]
], dtype=numpy.float32)

color = numpy.array([
    [0.9, 0.5, 0.5, 1.0],
    [0.8, 0.8, 0.8, 1.0],
    [0.8, 0.8, 0.8, 1.0],
    [0.5, 0.9, 0.5, 1.0]
], dtype=numpy.float32)

index = numpy.array([
    [0, 1, 3, 2]
], dtype=numpy.uint32)

ospray.init(sys.argv)

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


camera = ospray.Camera('perspective')
camera.set_param('aspect', W/H)
camera.set_param('position', cam_pos)
camera.set_param('direction', cam_view)
camera.set_param('up', cam_up)
camera.commit()

mesh = ospray.Geometry('mesh')
mesh.set_param('vertex.position', ospray.copied_data_constructor_vec(vertex))
mesh.set_param('vertex.texcoord', ospray.copied_data_constructor_vec(texcoord))
#mesh.set_param('vertex.color', ospray.copied_data_constructor_vec(color))
mesh.set_param('index', ospray.copied_data_constructor_vec(index))
mesh.commit()

gmodel = ospray.GeometricModel(mesh)
gmodel.commit()

group = ospray.Group()
group.set_param('geometry', [gmodel])
group.commit()

instance = ospray.Instance(group)
instance.commit()

TW = TH = 16
numpy.random.seed(123456)

filter = ospray.OSP_TEXTURE_FILTER_BILINEAR

if False:
    checker = numpy.indices((TW,TH)).sum(axis=0) % 2
    checker = 255 * checker.astype(numpy.uint8)
    pixels = numpy.zeros((TW,TH,3), dtype=numpy.uint8)
    pixels[..., 0] = checker
    pixels[..., 1] = checker
    pixels[..., 2] = checker
    data = ospray.copied_data_constructor_vec(pixels)
    format = ospray.OSP_TEXTURE_RGB8
    # XXX segfault
    #filter = ospray.OSP_TEXTURE_FILTER_NEAREST
elif False:
    pixels = numpy.random.rand(TW,TH,3).astype(numpy.float32)
    format = ospray.OSP_TEXTURE_RGB32F
    data = ospray.copied_data_constructor_vec(pixels)
elif True:
    img = Image.open(os.path.join(scriptdir, 'teaser_materials.jpg'))
    img = img.transpose(Image.FLIP_TOP_BOTTOM)
    pixels = numpy.array(img)
    data = ospray.copied_data_constructor_vec(pixels)
    format = ospray.OSP_TEXTURE_RGB8
    
texture = ospray.Texture('texture2d')
texture.set_param('format', format)
texture.set_param('data', data)
texture.set_param('filter', filter)
texture.commit()

material = ospray.Material('pathtracer', 'obj')
material.set_param('map_kd', texture)
material.commit()

gmodel.set_param('material', material)
gmodel.commit()

world = ospray.World()
world.set_param('instance', [instance])

light = ospray.Light('ambient')
#light.set_param('color', (1, 0.8, 0.8))
light.commit()

world.set_param('light', [light])
world.commit()
#print(world.get_bounds())

renderer = ospray.Renderer('pathtracer')
renderer.set_param('backgroundColor', (1.0, 1.0, 1.0, 1.0))
renderer.commit()

format = ospray.OSP_FB_SRGBA
channels = int(ospray.OSP_FB_COLOR) | int(ospray.OSP_FB_ACCUM) #| int(ospray.OSP_FB_DEPTH) | int(ospray.OSP_FB_VARIANCE)

framebuffer = ospray.FrameBuffer(W, H, format, channels)
framebuffer.clear()

future = framebuffer.render_frame(renderer, camera, world)
future.wait()

for frame in range(3):
    future = framebuffer.render_frame(renderer, camera, world)
    future.wait()

colors = framebuffer.get(ospray.OSP_FB_COLOR, (W,H), format)

img = Image.frombuffer('RGBA', (W,H), colors, 'raw', 'RGBA', 0, 1)
img = img.transpose(Image.FLIP_TOP_BOTTOM)
img.save('texture.png')
