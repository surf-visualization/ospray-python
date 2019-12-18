#!/usr/bin/env python
import sys
import numpy
from PIL import Image
import ospray

W = 1024
H = 768

cam_pos = numpy.array((0.0, 0.0, 0.0), dtype=numpy.float32)
cam_up = numpy.array((0.0, 1.0, 0.0), dtype=numpy.float32)
cam_view = numpy.array((0.1, 0.0, 1.0), dtype=numpy.float32)

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
mesh.set_param('vertex.position', ospray.Data(vertex))
mesh.set_param('vertex.color', ospray.Data(color))
mesh.set_param('index', ospray.Data(index))
mesh.commit()

gmodel = ospray.GeometricModel(mesh)
gmodel.commit()

group = ospray.Group()
group.set_param('geometry', ospray.Data(gmodel))
group.commit()

instance = ospray.Instance(group)
instance.commit()

world = ospray.World()
world.set_param('instance', ospray.Data(instance))

light = ospray.Light('ambient')
light.commit()

world.set_param('light', ospray.Data(light))
world.commit()

renderer = ospray.Renderer('scivis')
renderer.set_param('aoSamples', 1)
renderer.set_param('bgColor', 1.0)
renderer.commit()

format = ospray.OSP_FB_SRGBA
channels = int(ospray.OSP_FB_COLOR) | int(ospray.OSP_FB_ACCUM) | int(ospray.OSP_FB_DEPTH) | int(ospray.OSP_FB_VARIANCE)

framebuffer = ospray.FrameBuffer((W,H), format, channels)
framebuffer.clear()

future = framebuffer.render_frame(renderer, camera, world)
future.wait()
print(future.is_ready())

for frame in range(10):
    framebuffer.render_frame(renderer, camera, world)

colors = framebuffer.get(ospray.OSP_FB_COLOR, (W,H), format)
print(colors.shape)
#print(colors)

img = Image.frombuffer('RGBA', (W,H), colors, 'raw', 'RGBA', 0, 1)
img.save('colors.png')

#depth = framebuffer.get(ospray.OSP_FB_DEPTH, (W,H), format)
#print(depth.shape)
#print(numpy.min(depth), numpy.max(depth))
#
#img = Image.frombuffer('L', (W,H), depth, 'raw', 'L', 0, 1)
#img.save('depth.tif')
