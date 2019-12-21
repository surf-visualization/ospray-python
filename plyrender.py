#!/usr/bin/env python
import sys
import numpy
from PIL import Image
import ospray
# Needs https://github.com/paulmelis/blender-ply-import
from readply import readply

W = 1024
H = 768

ospray.init(sys.argv)

plyfile = sys.argv[1]

plymesh = readply(plyfile)

print('%d vertices, %d faces' % (plymesh['num_vertices'], plymesh['num_faces']))

loop_length = plymesh['loop_length']

minn, maxn = numpy.min(loop_length), numpy.max(loop_length)
print('Polygon size range: [%d, %d]' % (minn, maxn))

if minn > 3 or minn != maxn:
    print('Can only handle pure-triangle meshes, sorry...')
    sys.exit(-1)
    
vertices = plymesh['vertices']
vertices = vertices.reshape((-1, 3))

indices = plymesh['faces']
indices = indices.reshape((-1, 3))

mesh = ospray.Geometry('mesh')
mesh.set_param('vertex.position', ospray.Data(vertices))
#mesh.set_param('vertex.color', ospray.Data(color))
mesh.set_param('index', ospray.Data(indices))
mesh.commit()

gmodel = ospray.GeometricModel(mesh)
gmodel.commit()

group = ospray.Group()
group.set_param('geometry', [gmodel])
group.commit()

bound = group.get_bounds()
bound = numpy.array(bound, dtype=numpy.float32).reshape((-1, 3))
print(bound)

center = 0.5*(bound[0] + bound[1])
print(center)
    
position = center + 3*(bound[1] - center)

cam_pos = position
cam_up = (0.0, 0.0, 1.0)
cam_view = center - position

camera = ospray.Camera('perspective')
camera.set_param('aspect', W/H)
camera.set_param('position', cam_pos)
camera.set_param('direction', cam_view)
camera.set_param('up', cam_up)
camera.commit()

instance = ospray.Instance(group)
instance.commit()

material = ospray.Material('pathtracer', 'Metal')
material.set_param('eta', (0.07, 0.37, 1.5))
material.set_param('k', (3.7, 2.3, 1.7))
material.set_param('roughness', 0.5)
material.commit()

# Broken?
#material = ospray.Material('pathtracer', 'OBJMaterial')
#material.set_param('Kd', (0, 0, 1))
#material.set_param('Ns', 1.0)
#material.commit()

gmodel.set_param('material', material)
gmodel.commit()

world = ospray.World()
world.set_param('instance', [instance])

light1 = ospray.Light('ambient')
light1.set_param('color', (1, 1, 1))
light1.set_param('intensity', 0.2)
light1.commit()

light2 = ospray.Light('distant')
light2.set_param('direction', cam_view)
light2.set_param('intensity', 0.8)
light2.commit()

lights = [light1, light2]

world.set_param('light', lights)
world.commit()
print('World bound', world.get_bounds())

renderer = ospray.Renderer('pathtracer')
renderer.set_param('bgColor', 1.0)
renderer.commit()

format = ospray.OSP_FB_SRGBA
channels = int(ospray.OSP_FB_COLOR) | int(ospray.OSP_FB_ACCUM) | int(ospray.OSP_FB_DEPTH) | int(ospray.OSP_FB_VARIANCE)

framebuffer = ospray.FrameBuffer((W,H), format, channels)
framebuffer.clear()

future = framebuffer.render_frame(renderer, camera, world)
future.wait()
#print(future.is_ready())

for frame in range(10):
    future = framebuffer.render_frame(renderer, camera, world)
    future.wait()
    
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
