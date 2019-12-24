#!/usr/bin/env python
import sys, getopt, os
import numpy
from PIL import Image
import ospray

W = 1920
H = 1080
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
isovalue = None
grid_spacing = numpy.ones(3, dtype=numpy.float32)
samples = 4
voxel_type = None
value_range = None

optlist, args = getopt.getopt(argv[1:], 'd:D:i:I:l:ps:S:v:')

for o, a in optlist:
    if o == '-d':
        dimensions = tuple(map(int, a.split(',')))
        assert len(dimensions) == 3
    elif o == '-D':
        dataset_name = a
    elif o == '-i':
        isovalue = float(a)
    elif o == '-I':
        W, H = map(int, a.split(','))
    elif o == '-l':
        subvision_level = float(a)
    elif o == '-p':
        RENDERER = 'pathtracer'
    elif o == '-s':
        grid_spacing = tuple(map(float, a.split(',')))
        assert len(grid_spacing) == 3
        grid_spacing = numpy.array(grid_spacing, dtype=numpy.float32)
    elif o == '-S':
        samples = int(a)
    elif o == '-v':
        value_range = tuple(map(float, a.split(',')))

volfile = args[0]

# Read file

ext = os.path.splitext(volfile)[-1]

if ext == '.raw':
    assert dimensions is not None and 'Set dimensions with -d x,y,z'
    data = numpy.fromfile(volfile, dtype=numpy.uint8)    
    data = data.reshape(dimensions)
    voxel_type = ospray.OSP_UCHAR
    
elif ext in ['.h5', '.hdf5']:
    assert have_h5py and 'h5py module could not be loaded!'
    assert dataset_name and 'Set hdf5 dataset name with -D name'
    
    f = h5py.File(volfile, 'r')
    dset = f[dataset_name]
    data = numpy.array(dset[:])    
    f.close()
    
    dimensions = tuple(reversed(data.shape))
    #data = data.reshape(dimensions)
    
    if value_range is None:
        value_range = tuple(map(float, (numpy.min(data), numpy.max(data))))
    
    dtype = str(data.dtype)
        
    voxel_type = {
        'uint8': ospray.OSP_UCHAR,
        'float32': ospray.OSP_FLOAT,
        'float64': ospray.OSP_DOUBLE,
    }[dtype]
    
else:
    raise ValueError('Unknown file extension "%s"' % ext)
    
extent = dimensions * grid_spacing    

print('volume data', data.shape, data.dtype)
print('dimensions', dimensions)
print('value range', value_range)
print('spacing', grid_spacing)
print('extent', extent)

print(numpy.histogram(data))

assert value_range is not None and 'Set value range with -v min,max'

volume = ospray.Volume('structured_regular')
volume.set_param('dimensions', dimensions)
volume.set_param('gridSpacing', tuple(grid_spacing.tolist()))
volume.set_param('voxelType', voxel_type)
volume.set_param('data', data)
volume.commit()

# TF

T = 16

#tfcolors = numpy.array([[0, 0, 0], [0, 0, 1]], dtype=numpy.float32)
#tfopacities = numpy.array([0, 1], dtype=numpy.float32)

tfcolors = numpy.zeros((T,3), dtype=numpy.float32)
tfopacities = numpy.zeros(T, dtype=numpy.float32)

positions = numpy.array([
    0, 0.224, 0.312, 0.362, 1   
], dtype=numpy.float32)

colors = numpy.array([
    [0, 0, 1],
    [0, 0, 1],
    [0, 0, 0],
    [1, 1, 1],
    [1, 1, 1],
], dtype=numpy.float32)

opacities = numpy.array([
    1, 1, 1, 0, 0
], dtype=numpy.float32)

P = len(positions)

idx = 0
pos = 0.0
while idx < T:
    value = numpy.interp(pos, positions, numpy.arange(P))
        
    lowidx = int(value)
    highidx = min(lowidx + 1, P-1)
    factor = value - lowidx
    
    print(value, lowidx, highidx, factor)

    tfcolors[idx] = (1-factor) * colors[lowidx] + factor * colors[highidx]
    tfopacities[idx] = (1-factor) * opacities[lowidx] + factor * opacities[highidx]
    
    idx += 1
    pos += 1.0/(T-1)
    
transfer_function = ospray.TransferFunction('piecewise_linear')
transfer_function.set_param('color', tfcolors)
transfer_function.set_param('opacity', tfopacities)
transfer_function.set_param('valueRange', tuple(value_range))
transfer_function.commit()

vmodel = ospray.VolumetricModel(volume)
vmodel.set_param('transferFunction', transfer_function)
vmodel.commit()

# Isosurface rendered

if isovalue is not None:

    isovalues = numpy.array([isovalue], dtype=numpy.float32)
    isosurface = ospray.Geometry('isosurfaces')
    isosurface.set_param('isovalue', isovalues)
    isosurface.set_param('volume', vmodel)
    isosurface.commit()

    material = ospray.Material(RENDERER, 'OBJMaterial')
    material.set_param('Kd', (0.5, 0.5, 1.0))
    material.set_param('d', 1.0)
    material.commit()

    gmodel = ospray.GeometricModel(isosurface)
    gmodel.set_param('material', material)
    gmodel.commit()

    ggroup = ospray.Group()
    ggroup.set_param('geometry', [gmodel])
    ggroup.commit()

    ginstance = ospray.Instance(ggroup)
    ginstance.set_param('xfm', ospray.affine3f.identity())
    #ginstance.set_param('xfm', ospray.affine3f.translate((dimensions[0],0,0)))
    ginstance.commit()

    instances = [ginstance]
    
else:
    
    # Volume rendered

    vgroup = ospray.Group()
    vgroup.set_param('volume', [vmodel])
    vgroup.commit()

    vinstance = ospray.Instance(vgroup)
    vinstance.set_param('xfm', ospray.affine3f.identity())
    vinstance.commit()

    instances = [vinstance]

# Lights

if RENDERER == 'pathtracer':

    light1 = ospray.Light('ambient')
    light1.set_param('color', (1.0, 1.0, 1.0))
    light1.set_param('intensity', 0.3)
    light1.commit()

    light2 = ospray.Light('distant')
    light2.set_param('direction', (2.0, 1.0, -1.0))
    light2.set_param('intensity', 0.7)
    light2.commit()

    lights = [light1, light2]

# World

world = ospray.World()
world.set_param('instance', instances)
if RENDERER == 'pathtracer':
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

#cam_pos = tuple(numpy.array((center[0], center[1], center[2]+bound[1][2]), dtype=numpy.float32).tolist())
#cam_pos = tuple(map(float, (dimensions[0], dimensions[1], 2*dimensions[2])))
#cam_view = (0.0, -1.0, 0.0)
#cam_up = (0.0, 0.0, 1.0)

center = 0.5*extent
cam_pos = numpy.array([center[0], center[1]-extent[1], 0.5*max(extent)], dtype=numpy.float32)
cam_view = center - cam_pos
cam_up = numpy.array([0, 0, 1], dtype=numpy.float32)

camera = ospray.Camera('perspective')
camera.set_param('aspect', W/H)
camera.set_param('fovy', 50.0)
camera.set_param('position', tuple(cam_pos.tolist()))
camera.set_param('direction', tuple(cam_view.tolist()))
camera.set_param('up', tuple(cam_up.tolist()))
camera.commit()

#camera = ospray.Camera('orthographic')
#camera.set_param('aspect', W/H)
#camera.set_param('height', dimensions[1]*1.1)
#camera.set_param('position', cam_pos)
#camera.set_param('direction', cam_view)
#camera.set_param('up', cam_up)
#camera.commit()

# Renderer

#pixel = numpy.ones((1,1,3), numpy.uint8)
#backplate = ospray.Texture("texture2d")
#backplate.set_param('format', ospray.OSP_TEXTURE_RGB8)
#backplate.set_param('data', ospray.texture_data_from_numpy_array(pixel))
#backplate.commit()

renderer = ospray.Renderer(RENDERER)
if isovalue is not None:
    renderer.set_param('bgColor', 1.0)
else:
    renderer.set_param('bgColor', (0.0, 0.0, 0.0))
    #renderer.set_param('backplate', backplate)
#if RENDERER == 'scivis':
#    renderer.set_param('volumeSamplingRate', 1.0)
renderer.commit()

# Framebuffer

format = ospray.OSP_FB_SRGBA
channels = int(ospray.OSP_FB_COLOR) | int(ospray.OSP_FB_ACCUM) | int(ospray.OSP_FB_DEPTH) | int(ospray.OSP_FB_VARIANCE)

framebuffer = ospray.FrameBuffer((W,H), format, channels)
framebuffer.clear()

# Render!

for frame in range(samples):
    future = framebuffer.render_frame(renderer, camera, world)
    future.wait()
    sys.stdout.write('.')
    sys.stdout.flush()
    
sys.stdout.write('\n')
    
colors = framebuffer.get(ospray.OSP_FB_COLOR, (W,H), format)
print(colors.shape)
#print(colors)

img = Image.frombuffer('RGBA', (W,H), colors, 'raw', 'RGBA', 0, 1)
img = img.transpose(Image.FLIP_TOP_BOTTOM)
img.save('colors.png')

#depth = framebuffer.get(ospray.OSP_FB_DEPTH, (W,H), format)
#print(depth.shape)
#print(numpy.min(depth), numpy.max(depth))
#
#img = Image.frombuffer('L', (W,H), depth, 'raw', 'L', 0, 1)
#img.save('depth.tif')
