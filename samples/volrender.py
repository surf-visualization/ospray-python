#!/usr/bin/env python
import sys, getopt, os, time
scriptdir = os.path.split(__file__)[0]
sys.path.insert(0, os.path.join(scriptdir, '..'))

import numpy
from PIL import Image
import ospray

t0 = time.time()

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

try:
    import vtk
    have_vtk = True
except ImportError:
    have_vtk = False

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

def usage():
    print('%s [options] file.raw|file.h5|file.hdf5' % sys.argv[0])
    print()
    print('Options:')
    print(' -b r,g,b[,a]                    Background color')
    print(' -d xdim,ydim,zdim               .raw dimensions')
    print(' -D dataset_name                 HDF5 dataset name')
    print(' -f axis,minidx,maxidx,value     Fill part of the volume with a specific value')
    print(' -g                              Simple gradual TF')
    print(' -i isovalue')
    print(' -I width,height                 Image resolution')
    print(' -p                              Use pathtracer (default: use scivis renderer)')
    print(' -s xs,ys,zs                     Grid spacing')
    print(' -S samples')
    print(' -v minval,maxval')
    print()

anisotropy = 0.0
bgcolor = (1.0, 1.0, 1.0, 1.0)
dimensions = None
dataset_name = None
gradual_tf = False
image_file = 'volume.png'
isovalue = None
grid_spacing = numpy.ones(3, dtype=numpy.float32)
samples = 4
set_value = None
value_range = None


try:
    optlist, args = getopt.getopt(argv[1:], 'a:b:d:D:f:gi:I:o:ps:S:t:v:')
except getopt.GetoptError as err:
    print(err)
    usage()
    sys.exit(2)

for o, a in optlist:
    if o == '-a':
        anisotropy = float(a)
    elif o == '-b':
        bgcolor = list(map(float, a.split(',')))
        assert len(bgcolor) in [3,4]
        if len(bgcolor) == 3:
            bgcolor.append(1.0)
        bgcolor = tuple(bgcolor)
    elif o == '-d':
        dimensions = tuple(map(int, a.split(',')))
        assert len(dimensions) == 3
    elif o == '-D':        
        dataset_name = a
    elif o == '-f':
        pp = a.split(',')
        assert len(pp) == 4
        set_value = (int(pp[0]), int(pp[1]), int(pp[2]), float(pp[3]))     
    elif o == '-g':
        gradual_tf = True
    elif o == '-i':
        isovalue = float(a)
    elif o == '-I':
        W, H = map(int, a.split(','))
    elif o == '-o':
        image_file = a
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

if len(args) == 0:
    usage()
    sys.exit(2)
    
volfile = args[0]

# Read file

extent = numpy.zeros((2,3), 'float32')

ext = os.path.splitext(volfile)[-1]

if ext == '.raw':
    assert dimensions is not None and 'Set dimensions with -d x,y,z'
    data = numpy.fromfile(volfile, dtype=numpy.uint8)    
    data = data.reshape(dimensions)
    
    extent[1] = dimensions * grid_spacing   
    
elif ext in ['.h5', '.hdf5']:
    assert have_h5py and 'h5py module could not be loaded!'
    assert dataset_name and 'Set hdf5 dataset name with -D name'
    
    f = h5py.File(volfile, 'r')
    dset = f[dataset_name]
    data = numpy.array(dset[:])    
    f.close()
    
    # KJI -> IJK
    data = numpy.swapaxes(data, 0, 2)
    dimensions = data.shape
    
    if value_range is None:
        value_range = tuple(map(float, (numpy.min(data), numpy.max(data))))
    
    extent[1] = dimensions * grid_spacing   
    
elif ext in ['.vtk', '.vti']:
    assert have_vtk and 'vtk module could not be loaded!'
    from vtk.numpy_interface import dataset_adapter
    
    if ext == '.vtk':    
        dr = vtk.vtkDataSetReader()
        dr.SetFileName(volfile)
        dr.Update()
        sp = dr.GetStructuredPointsOutput()
        
    else:
        dr = vtk.vtkXMLImageDataReader()
        dr.SetFileName(volfile)
        dr.Update()
        sp = dr.GetOutput()
        
    assert sp is not None
    print(sp)
    
    dimensions = sp.GetDimensions()
    extent = numpy.array(sp.GetExtent(), 'float32').reshape((3,2)).swapaxes(0,1)
    grid_spacing = numpy.array(sp.GetSpacing(), 'float32')
    
    scalar_type = sp.GetScalarTypeAsString()
    print('scalar_type', scalar_type)
        
    volume_do = dataset_adapter.WrapDataObject(sp)
    assert len(volume_do.PointData.keys()) > 0
    scalar_name = volume_do.PointData.keys()[0]
    data = volume_do.PointData[scalar_name]

    print('Point scalar data "%s"' % scalar_name, data)
    
    assert len(data.shape) == 1
    data = data.reshape(dimensions)
    assert len(data.shape) == 3
    
    value_range = tuple(map(float, (numpy.min(data), numpy.max(data))))
    
else:
    raise ValueError('Unknown file extension "%s"' % ext)    

print('volume data', data.shape, data.dtype)
print('dimensions', dimensions)
print('value range', value_range)
print('spacing', grid_spacing)
print('extent', extent)

assert value_range is not None and 'Set value range with -v min,max'

if set_value is not None:
    axis, minidx, maxidx, value = set_value
    assert axis in [0,1,2]
    if axis == 0:
        data[minidx:maxidx+1] = value
    elif axis == 1:
        data[:,minidx:maxidx+1] = value
    else:
        data[:,:,minidx:maxidx+1] = value

# Keep a reference to the numpy array around, as it will get
# deallocated otherwise
saved_data = data

data = ospray.data_constructor(data, is_shared=True)

volume = ospray.Volume('structuredRegular')
volume.set_param('gridSpacing', tuple(grid_spacing.tolist()))
volume.set_param('data', data)
volume.commit()

# TF

T = 16

if isovalue is not None:
    # Fixed color and opacity
    tfcolors = numpy.array([[0.8, 0.8, 0.8]], dtype=numpy.float32)
    tfopacities = numpy.array([1], dtype=numpy.float32)
    
elif gradual_tf:
    # Simple gradual TF
    tfcolors = numpy.array([[0, 0, 0], [0, 0, 1]], dtype=numpy.float32)
    tfopacities = numpy.array([0, 1], dtype=numpy.float32)
    
else:
    # Generate equidistant TF from sparse specification
    tfcolors = numpy.zeros((T,3), dtype=numpy.float32)
    tfopacities = numpy.zeros(T, dtype=numpy.float32)

    positions = numpy.array([
        0, 0.318, 0.462, 0.546, 1   
    ], dtype=numpy.float32)

    colors = numpy.array([
        [0, 0, 1],
        [0, 1, 0],
        [0.013, 0, 0.5],
        [0.229, 0, 0.5],
        [0.229, 0, 0.5],
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
    
print('tfcolors', tfcolors.shape)
print('tfopacities', tfopacities.shape)
    
transfer_function = ospray.TransferFunction('piecewiseLinear')
transfer_function.set_param('color', ospray.data_constructor_vec(tfcolors))
transfer_function.set_param('opacity', tfopacities)
transfer_function.set_param('valueRange', tuple(value_range))
transfer_function.commit()

vmodel = ospray.VolumetricModel(volume)
vmodel.set_param('transferFunction', transfer_function)
#vmodel.set_param('densityScale', 0.1)
vmodel.set_param('anisotropy', anisotropy)
vmodel.commit()

# Isosurface rendered

if isovalue is not None:
    
    isovalues = numpy.array([isovalue], dtype=numpy.float32)
    isosurface = ospray.Geometry('isosurface')
    isosurface.set_param('isovalue', isovalues)
    isosurface.set_param('volume', vmodel)
    isosurface.commit()

    material = ospray.Material(RENDERER, 'obj')
    material.set_param('kd', (0.5, 0.5, 1.0))
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

# World

world = ospray.World()
world.set_param('instance', instances)
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

center = 0.5*(extent[0] + extent[1])
cam_pos = numpy.array([center[0], center[1]-extent[1,1], 0.5*max(extent[1])], dtype=numpy.float32)
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

# Lights

if RENDERER == 'pathtracer':

    light1 = ospray.Light('ambient')
    light1.set_param('color', (1.0, 1.0, 1.0))
    light1.set_param('intensity', 0.3)
    light1.commit()

    light2 = ospray.Light('distant')
    light2.set_param('direction', tuple(cam_view.tolist()))#(2.0, 1.0, -1.0))
    light2.set_param('intensity', 0.7)
    light2.commit()

    lights = [light1, light2]

    world.set_param('light', lights)
    world.commit()

# Renderer

#pixel = numpy.ones((1,1,3), numpy.uint8)
#backplate = ospray.Texture("texture2d")
#backplate.set_param('format', ospray.OSP_TEXTURE_RGB8)
#backplate.set_param('data', ospray.data_constructor_vec(pixel))
#backplate.commit()

renderer = ospray.Renderer(RENDERER)
renderer.set_param('backgroundColor', bgcolor)

#if isovalue is not None:
#    renderer.set_param('bgColor', 1.0)
#else:
#    renderer.set_param('bgColor', (0.0, 0.0, 0.0))
#    #renderer.set_param('backplate', backplate)
if RENDERER == 'scivis':
    renderer.set_param('volumeSamplingRate', 1.0)
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
img = Image.frombuffer('RGBA', (W,H), colors, 'raw', 'RGBA', 0, 1)
img = img.transpose(Image.FLIP_TOP_BOTTOM)
img.save(image_file)

# XXX seems depths aren't stored on volume hits
#depth = framebuffer.get(ospray.OSP_FB_DEPTH, (W,H), format)
#print(depth.shape)
#print(numpy.min(depth), numpy.max(depth))
#depth[depth == numpy.inf] = 1000.0
#img = Image.frombuffer('F', (W,H), depth, 'raw', 'F', 0, 1)
#img = img.transpose(Image.FLIP_TOP_BOTTOM)
#img.save('depth.tif')

t1 = time.time()
print('All done in %.3fs' % (t1-t0))