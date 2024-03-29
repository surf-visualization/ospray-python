#!/usr/bin/env python
import sys, os
scriptdir = os.path.split(__file__)[0]
sys.path.insert(0, os.path.join(scriptdir, '..'))

import numpy
from PIL import Image
import ospray

W = 1024
H = 768

cam_pos = (0.0, 0.0, 0.0)
cam_up = (0.0, 1.0, 0.0)
cam_view = (0.1, 0.0, 1.0)

vertex = numpy.array([
   [-1.0, -1.0, 3.0],
   [-1.0, 1.0, 3.0],
   [1.0, -1.0, 3.0],
   [0.1, 0.1, 0.3]
], dtype=numpy.float32)

color = numpy.array([
    [0.9, 0.5, 0.5, 1.0],
    [0.8, 0.8, 0.8, 1.0],
    [0.8, 0.8, 0.8, 1.0],
    [0.5, 0.9, 0.5, 1.0]
], dtype=numpy.float32)

index = numpy.array([
    [0, 1, 2], [1, 2, 3]
], dtype=numpy.uint32)

ospray.init(sys.argv)

camera = ospray.Camera('perspective')
camera.set_param('aspect', W/H)
camera.set_param('position', cam_pos)
camera.set_param('direction', cam_view)
camera.set_param('up', cam_up)
camera.commit()

mesh = ospray.Geometry('mesh')
data = ospray.copied_data_constructor_vec(vertex)
print(data)
mesh.set_param('vertex.position', data)
mesh.set_param('vertex.color', ospray.copied_data_constructor_vec(color))
mesh.set_param('index', ospray.copied_data_constructor_vec(index))
mesh.commit()

gmodel = ospray.GeometricModel(mesh)
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
renderer.set_param('aoSamples', 1)
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
img.save('colors.png')
