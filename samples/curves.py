#!/usr/bin/env python
import sys, os
scriptdir = os.path.split(__file__)[0]
sys.path.insert(0, os.path.join(scriptdir, '..'))

import numpy
from PIL import Image
import ospray

W = 1024
H = 768

cam_pos = (1.5, 1.5, 5.0)
cam_up = (0.0, 1.0, 0.0)
cam_view = (0.0, 0.0, -1.0)

vertex = numpy.array([
    [0, 3, 0],
    [3, 3, 0],
    [0, 0, 0],
    [3, 0, 0],
], dtype=numpy.float32)

vertex_and_radius = numpy.array([
    [0, 3, 0, 0],
    [3, 3, 0, 0.1],
    [0, 0, 0, 0.1],
    [3, 0, 0, 0.2],    
], dtype=numpy.float32)

color = numpy.array([
    [1, 0, 0, 1],
    [0, 1, 0, 1],
    [0, 1, 1, 1],
    [0, 0, 1, 1],
], dtype=numpy.float32)

index = numpy.array([
    0, 1, 
    1, 2,
    2, 3
], dtype=numpy.uint32)

ospray.init(sys.argv)

def error_callback(error, details):
    print('OSPRAY ERROR: %d (%s)' % (error, details))
    
def status_callback(message):
    print('OSPRAY STATUS: %s' % message)
    
ospray.set_error_callback(error_callback)
ospray.set_status_callback(status_callback)

camera = ospray.Camera('perspective')
camera.set_param('aspect', W/H)
camera.set_param('position', cam_pos)
camera.set_param('direction', cam_view)
camera.set_param('up', cam_up)
camera.commit()

curves = ospray.Geometry('curve')
if False:
    curves.set_param('vertex.position', ospray.copied_data_constructor_vec(vertex))
    curves.set_param('radius', 0.1)
else:
    curves.set_param('vertex.position_radius', ospray.copied_data_constructor_vec(vertex_and_radius))
curves.set_param('vertex.color', ospray.copied_data_constructor_vec(color))
curves.set_param('index', ospray.copied_data_constructor(index))
curves.set_param('type', ospray.OSP_ROUND)
curves.set_param('basis', ospray.OSP_LINEAR)
curves.commit()

gmodel = ospray.GeometricModel(curves)
gmodel.commit()

group = ospray.Group()
group.set_param('geometry', [gmodel])
group.commit()

instance = ospray.Instance(group)
instance.commit()

material = ospray.Material('pathtracer', 'obj')
material.commit()

gmodel.set_param('material', material)
gmodel.commit()

world = ospray.World()
world.set_param('instance', [instance])

light = ospray.Light('ambient')
light.set_param('color', (1.0, 1, 1))
light.set_param('intensity', 1.0)
light.commit()

world.set_param('light', [light])
world.commit()

renderer = ospray.Renderer('pathtracer')
renderer.set_param('backgroundColor', (1.0, 1.0, 1.0, 1.0))
renderer.commit()

format = ospray.OSP_FB_SRGBA
channels = int(ospray.OSP_FB_COLOR) | int(ospray.OSP_FB_ACCUM) | int(ospray.OSP_FB_DEPTH) | int(ospray.OSP_FB_VARIANCE)

framebuffer = ospray.FrameBuffer(W, H, format, channels)
framebuffer.clear()

for frame in range(8):
    future = framebuffer.render_frame(renderer, camera, world)
    future.wait()
    print('[%d] %.6f' % (frame, framebuffer.get_variance()))
    
res = framebuffer.pick(renderer, camera, world, 0.5, 0.5)
if res.has_hit:
    print('pick world pos', res.world_position)
    print('pick instance', res.instance)
    print('pick model', res.model)
    print('pick prim ID', res.prim_id)    
    assert res.instance.same_handle(instance)
    assert res.model.same_handle(gmodel)

colors = framebuffer.get(ospray.OSP_FB_COLOR, (W,H), format)
print(colors.shape)

img = Image.frombuffer('RGBA', (W,H), colors, 'raw', 'RGBA', 0, 1)
img = img.transpose(Image.FLIP_TOP_BOTTOM)
img.save('curves.png')
