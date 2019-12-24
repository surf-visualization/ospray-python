#!/usr/bin/env python
import sys, getopt, os
import numpy
from PIL import Image
import ospray

W = 1024
H = 768
RENDERER = 'scivis'

argv = ospray.init(sys.argv)

try:
    # Needs to go after ospInit apparently, as get a lockup during exit otherwise
    import h5py
    have_h5py = True
except ImportError:
    have_h5py = False

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

dimensions = None
dataset_name = None
force_subdivision_mesh = False
subvision_level = 5.0
voxel_type = None

optlist, args = getopt.getopt(argv[1:], 'd:D:l:s')

for o, a in optlist:
    if o == '-d':
        dimensions = tuple(map(int, a.split(',')))
        assert len(dimensions) == 3
    elif o == '-D':
        dataset_name = a
    elif o == '-l':
        subvision_level = float(a)
    elif o == '-s':
        force_subdivision_mesh = True

volfile = args[0]

# Read file

ext = os.path.splitext(volfile)[-1]

if ext == '.raw':
    assert dimensions is not None
    data = numpy.fromfile(volfile, dtype=numpy.uint8)    
    data = data.reshape(dimensions)
    voxel_type = ospray.OSP_UCHAR
    
elif ext in ['.h5', '.hdf5']:
    assert have_h5py and 'Need h5py module!'
    assert dataset_name and 'Dataset name needs to be set with -D'
    f = h5py.File(volfile, 'r')
    dset = f[dataset_name]
    data = numpy.array(dset[:])
    dimensions = data.shape
    f.close()
    
    dtype = str(data.dtype)
        
    voxel_type = {
        'float32': ospray.OSP_FLOAT
    }[dtype]

print('volume', dimensions, data.shape, data.dtype)

# Volume rendered

tfcolors = numpy.array([[0, 0, 0], [1, 1, 1]], dtype=numpy.float32)
tfopacities = numpy.array([0, 1], dtype=numpy.float32)

transfer_function = ospray.TransferFunction('piecewise_linear')
transfer_function.set_param('color', tfcolors)
transfer_function.set_param('opacity', tfopacities)
transfer_function.set_param('valueRange', (0.0, 255.0))
transfer_function.commit()

volume = ospray.Volume('structured_regular')
volume.set_param('dimensions', dimensions)
volume.set_param('voxelType', voxel_type)
volume.set_param('data', data)
volume.commit()

vmodel = ospray.VolumetricModel(volume)
vmodel.set_param('transferFunction', transfer_function)
vmodel.commit()

vgroup = ospray.Group()
vgroup.set_param('volume', [vmodel])
vgroup.commit()

vinstance = ospray.Instance(vgroup)
vinstance.set_param('xfm', ospray.affine3f.identity())
vinstance.commit()

# Isosurface rendered

isovalues = numpy.array([80.0], dtype=numpy.float32)
isosurface = ospray.Geometry('isosurfaces')
isosurface.set_param('isovalue', isovalues)
isosurface.set_param('volume', vmodel)
isosurface.commit()

# Broken?
material = ospray.Material(RENDERER, 'OBJMaterial')
material.set_param('Kd', (0.0, 0.0, 1.0))
material.set_param('d', 1.0)
#material.set_param('Ns', 1.0)
material.commit()

gmodel = ospray.GeometricModel(isosurface)
gmodel.set_param('material', material)
gmodel.commit()

ggroup = ospray.Group()
ggroup.set_param('geometry', [gmodel])
ggroup.commit()

ginstance = ospray.Instance(ggroup)
ginstance.set_param('xfm', ospray.affine3f.translate((dimensions[0],0,0)))
ginstance.commit()

instances = [vinstance, ginstance]

# Lights

light1 = ospray.Light('ambient')
light1.set_param('color', (1.0, 1.0, 1.0))
light1.set_param('intensity', 0.4)
light1.commit()

light2 = ospray.Light('distant')
light2.set_param('direction', (1.0, 1.0, -1.0))
light2.set_param('intensity', 0.6)
light2.commit()

lights = [light1, light2]

# World

world = ospray.World()
world.set_param('instance', instances)
world.set_param('light', lights)
world.commit()

bound = world.get_bounds()
bound = numpy.array(bound, dtype=numpy.float32).reshape((-1, 3))
center = 0.5*(bound[0] + bound[1])

print('World bound', bound)
print('World center', center)

# Camera

#position = center + 3*(bound[1] - center)
#cam_pos = position
#cam_up = (0.0, 0.0, 1.0)
#cam_view = center - position

cam_pos = numpy.array((center[0], center[1], center[2]+bound[1][2]), dtype=numpy.float32)
cam_view = (0.0, 0.0, -1.0)
cam_up = (0.0, 1.0, 0.0)

camera = ospray.Camera('perspective')
camera.set_param('aspect', W/H)
camera.set_param('fovy', 50.0)
camera.set_param('position', tuple(cam_pos.tolist()))
camera.set_param('direction', cam_view)
camera.set_param('up', cam_up)
camera.commit()

# Renderer

renderer = ospray.Renderer(RENDERER)
renderer.set_param('bgColor', 1.0)
renderer.commit()

# Framebuffer

format = ospray.OSP_FB_SRGBA
channels = int(ospray.OSP_FB_COLOR) | int(ospray.OSP_FB_ACCUM) | int(ospray.OSP_FB_DEPTH) | int(ospray.OSP_FB_VARIANCE)

framebuffer = ospray.FrameBuffer((W,H), format, channels)
framebuffer.clear()

# Render!

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
