#!/usr/bin/env python
"""
Isosurface rendering results in black image and warning

OSPRAY STATUS: ospray::Isosurfaces deprecated parameter use. Isosurfaces will begin taking an OSPVolume directly, with appearance set through the GeometricModel instead.

OSPRAY STATUS: ospray::Isosurfaces created: #primitives=1

Even though scene setup appears to be correct
"""
# E.g.
# ./samples/volrender.py -s 1,1,2 -d 256,256,84 -v 0,255 -t ~/models/brain/carnival.trn ~/models/brain/brain256_256_84_8.raw 
# ./samples/volrender.py -i 128 -s 1,1,2 -d 256,256,84 -v 0,255 -t ~/models/brain/carnival.trn ~/models/brain/brain256_256_84_8.raw 
# ./samples/volrender.py -c 'z<0.035' -i 4 -D u -s 0.009,0.009,0.005 u_00659265.h5
# ./samples/volrender.py -b 0,0,0 -c 'z<0.035' -D u -t tf2.trn -s 0.009,0.009,0.005 u_00659265.h5
import sys, getopt, os, time
import importlib.util
from math import sqrt, tan, atan, radians, degrees
scriptdir = os.path.split(__file__)[0]
sys.path.insert(0, os.path.join(scriptdir, '..'))

import numpy
from PIL import Image
import ospray

t0 = time.time()

W = 960
H = 540

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
    
ospray.set_error_callback(error_callback)
ospray.set_status_callback(status_callback)

device = ospray.get_current_device()
device.set_param('logLevel', 1)
#device.set_param('logOutput', 'cerr')
#device.set_param('errorOutput', 'cerr')
device.commit()

# Parse arguments

anisotropy = 0.0
bgcolor = (1.0, 1.0, 1.0, 1.0)
show_domain_bbox = False
clipping_gmodel = None
dimensions = None
dataset_name = None
tf_mode = 'default'
tf_file = None
image_file = 'volume.png'
isovalue = None
grid_spacing = numpy.ones(3, dtype=numpy.float32)
samples = 4
set_value = None
value_range = None
display_result = False
show_histogram = False
camera_configuration = None
camera_fov_string = None
extra_scene_script = None


def usage():
    print('%s [options] file.raw|file.h5|file.hdf5' % sys.argv[0])
    print()
    print('Options:')
    print(' -a anisotropy                   Volume anisotropy')
    print(' -b r,g,b[,a]                    Background color')
    print(' -B                              Show domain bounding box')
    print(' -c [xyz][<>]<value>             Set clip plane')
    print(' -C px,py,pz,lx,ly,lz,x|y|z,h|v<fov>')
    print('                                 Set camera (position, lookat, up axis, horizontal|vertical fov)')
    print(' -d xdim,ydim,zdim               .raw file dimensions')
    print(' -D dataset_name                 HDF5 dataset name')
    print(' -f axis,minidx,maxidx,value     Fill part of the volume with a specific value')    
    print(' -H                              Print histogram of volume data')
    print(' -i isovalue                     Render as isosurface instead of volume')
    print(' -I width,height                 Image resolution')
    print(' -o output-image                 Output file (default: volume.png)')
    print(' -p                              Use pathtracer (default: use scivis renderer)')
    print(' -s xs,ys,zs                     Grid spacing')
    print(' -S samples                      Samples per pixel (default: %d)' % samples)
    print(' -t <default>|<linear>|<file.trn> Set transfer function')
    print(' -v minval,maxval                Volume value range')
    print(' -x                              Display image after rendering (uses tkinter)')
    print(' -X script.py                    Execute script to set up extra scene elements')
    print()
    print('When reading a .raw file 8-bit unsigned integers are assumed')
    print()

try:
    optlist, args = getopt.getopt(argv[1:], 'a:b:Bc:C:d:D:f:Hi:I:o:ps:S:t:v:xX:')
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
    elif o == '-B':
        show_domain_bbox = True
    elif o == '-c':
        assert a[0] in ['x', 'y', 'z']
        axis = a[0]
        a = a[1:]        
        if a[0] == '<':            
            value = float(a[1:])            
            invert_clip_plane_normal = True
        elif a[0] == '>':
            value = float(a[1:])
            invert_clip_plane_normal = False
        else:
            assert False and "Invalid clip plane format"
        if axis == 'x':
            clip_plane_coefficients = (-1, 0, 0, -value)
        elif axis == 'y':
            clip_plane_coefficients = (0, -1, 0, -value)
        else:
            clip_plane_coefficients = (0, 0, -1, -value)
        c = numpy.array([clip_plane_coefficients], 'float32')        
        g = ospray.Geometry('plane')        
        g.set_param('plane.coefficients', ospray.copied_data_constructor_vec(c))
        g.commit()
        clipping_gmodel = ospray.GeometricModel(g)
        clipping_gmodel.set_param('invertNormals', invert_clip_plane_normal)
        clipping_gmodel.commit()
    elif o == '-C':
        pp = a.split(',')
        assert len(pp) == 8
        position = numpy.array(pp[:3], 'float32')
        lookat = numpy.array(pp[3:6], 'float32')
        direction = lookat - position
        upaxis = pp[6]
        assert upaxis in ['x','y','z']
        hv = pp[7][0]
        assert hv in ['h', 'v']
        camera_fov_string = pp[7]
        camera_configuration = {
            'position': position,
            'direction': direction, 
            'up': { 
                'x': numpy.array((1., 0., 0.), 'float32'),
                'y': numpy.array((0., 1., 0.), 'float32'),
                'z': numpy.array((0., 0., 1.), 'float32')
            }[upaxis]
        }        
    elif o == '-d':
        dimensions = tuple(map(int, a.split(',')))
        assert len(dimensions) == 3
    elif o == '-D':        
        dataset_name = a
    elif o == '-f':
        pp = a.split(',')
        assert len(pp) == 4
        set_value = (int(pp[0]), int(pp[1]), int(pp[2]), float(pp[3]))     
    elif o == '-H':
        show_histogram = True
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
    elif o == '-t':
        if os.path.isfile(a):
            tf_mode = 'file'
            tf_file = a
        else:
            assert a in ['default', 'linear']
            tf_mode = a
    elif o == '-v':
        value_range = tuple(map(float, a.split(',')))
    elif o == '-x':
        display_result = True
    elif o == '-X':
        extra_scene_script = a

if len(args) == 0:
    usage()
    sys.exit(2)
    
volfile = args[0]

if camera_fov_string is not None:
    # Compute V fov, using aspect ratio
    hv = camera_fov_string[0]
    assert hv in ['h', 'v']
    fov = float(camera_fov_string[1:])
    if hv == 'h':
        ipw = 2*tan(radians(fov/2))
        aspect = W / H
        iph = ipw / aspect
        fov = degrees(2*atan(iph/2))
    camera_configuration['vfov'] = fov

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

minx, miny, minz = extent[0]
maxx, maxy, maxz = extent[1]
diagonal_size = sqrt((maxx-minx)**2 + (maxy-miny)**2 + (maxz-minz)**2)

print('volume data', data.shape, data.dtype)
print('dimensions', dimensions)
print('value range', value_range)
print('spacing', grid_spacing)
print('extent', extent)

if show_histogram:
    print(numpy.histogram(data, bins=30))

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

data = ospray.shared_data_constructor(data)

volume = ospray.Volume('structuredRegular')
volume.set_param('gridSpacing', tuple(grid_spacing.tolist()))
volume.set_param('data', data)
volume.commit()

# TF

def generate_tf(values, colors, opacities, T=16):
    """
    The values, colors and opacities arrays together sparsely define
    the transfer function.
    """

    assert value_range is not None
    
    #print(values)
    #print(colors)
    #print(opacities)
        
    P = values.shape[0]
    assert colors.shape[0] == P and colors.shape[1] == 3
    assert opacities.shape[0] == P
    
    # Generate equidistant TF from sparse specification
    tfcolors = numpy.zeros((T,3), dtype=numpy.float32)
    tfopacities = numpy.zeros(T, dtype=numpy.float32)

    idx = 0
    pos = 0.0
    pos_step = value_range[-1]/(T-1)
    while idx < T:
        valueidx = numpy.interp(pos, values, numpy.arange(P))
            
        lowidx = int(valueidx)
        highidx = min(lowidx + 1, P-1)
        factor = valueidx - lowidx
        
        #print(value, valueidx, lowidx, highidx, factor)

        tfcolors[idx] = (1-factor) * colors[lowidx] + factor * colors[highidx]
        tfopacities[idx] = (1-factor) * opacities[lowidx] + factor * opacities[highidx]
        
        idx += 1
        pos += pos_step
        
    return tfcolors, tfopacities
    

if isovalue is not None:
    # Fixed color and opacity for isosurface
    tfcolors = numpy.array([[0.8, 0.8, 0.8]], dtype=numpy.float32)
    tfopacities = numpy.array([1], dtype=numpy.float32)
    
elif tf_mode == 'file':
    # Read a .trn file
    # <minval> <maxval>
    # <value> <r> <g> <b> <a>
    lines = [l.strip() for l in open(tf_file, 'rt').readlines() if l.strip() != '' and l[0] != '#']    
    
    if value_range is None:
        value_range = list(map(float, lines[0].split(' ')))
    lines = lines[1:]
    
    N = len(lines)
    assert N >= 2
    
    values = numpy.zeros(N, dtype=numpy.float32)
    colors = numpy.zeros((N,3), dtype=numpy.float32)
    opacities = numpy.zeros(N, dtype=numpy.float32)
    
    for idx, line in enumerate(lines):
        pp = list(map(float, line.split()))
        assert len(pp) == 5
        values[idx] = pp[0]
        colors[idx] = (pp[1], pp[2], pp[3])
        opacities[idx] = pp[4]
        
    tfcolors, tfopacities = generate_tf(values, colors, opacities)    
    
elif tf_mode == 'linear':
    # Simple linear TF
    tfcolors = numpy.array([[0, 0, 0], [0, 0, 1]], dtype=numpy.float32)
    tfopacities = numpy.array([0, 1], dtype=numpy.float32)
    
elif tf_mode == 'default':

    values = numpy.array([
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
    
    tfcolors, tfopacities = generate_tf(values, colors, opacities)
    
#print('tfcolors', tfcolors.shape)
#print('tfopacities', tfopacities.shape)

#print('TF:')
#print(value_range)
#print(tfcolors)
#print(tfopacities)
    
if isovalue is not None:

    # Isosurface rendered
    
    isovalues = numpy.array([isovalue], dtype=numpy.float32)
    isosurface = ospray.Geometry('isosurface')
    isosurface.set_param('isovalue', isovalues)
    isosurface.set_param('volume', volume)
    isosurface.commit()

    material = ospray.Material(RENDERER, 'obj')
    material.set_param('kd', (0.5, 0.5, 1.0))
    material.set_param('d', 1.0)
    material.commit()

    gmodel = ospray.GeometricModel(isosurface)
    #gmodel.set_param('material', material)
    gmodel.commit()

    ggroup = ospray.Group()
    ggroup.set_param('geometry', [gmodel])
    if clipping_gmodel is not None:
        ggroup.set_param('clippingGeometry', [clipping_gmodel])
    ggroup.commit()

    ginstance = ospray.Instance(ggroup)
    # XXX why specify transform at all?
    ginstance.set_param('transform', ospray.mat4.identity())
    #ginstance.set_param('transform', ospray.mat4.translate(dimensions[0], 0, 0))
    ginstance.commit()

    instances = [ginstance]
    
else:
    
    # Volume rendered

    transfer_function = ospray.TransferFunction('piecewiseLinear')
    transfer_function.set_param('color', ospray.copied_data_constructor_vec(tfcolors))
    transfer_function.set_param('opacity', ospray.copied_data_constructor(tfopacities))
    transfer_function.set_param('valueRange', tuple(value_range))
    transfer_function.commit()

    vmodel = ospray.VolumetricModel(volume)
    vmodel.set_param('transferFunction', transfer_function)
    #vmodel.set_param('densityScale', 0.01)
    vmodel.set_param('anisotropy', anisotropy)
    vmodel.commit()

    vgroup = ospray.Group()
    vgroup.set_param('volume', [vmodel])
    if clipping_gmodel is not None:
        vgroup.set_param('clippingGeometry', [clipping_gmodel])
    vgroup.commit()

    vinstance = ospray.Instance(vgroup)
    # XXX why specify transform at all?
    vinstance.set_param('transform', ospray.mat4.identity())
    vinstance.commit()

    instances = [vinstance]

# Domain bbox

if show_domain_bbox:
    
    radius = diagonal_size / 400

    # Could use the 8 corner vertices for specifying the box, but
    # this way we can color the edges corresponding to the axes
    vertices = numpy.array([
        (minx, miny, minz), (maxx, miny, minz),     # X axis
        (maxx, miny, minz), (maxx, maxy, minz),
        (maxx, maxy, minz), (minx, maxy, minz),
        (minx, maxy, minz), (minx, miny, minz),     # Y axis

        (minx, miny, maxz), (maxx, miny, maxz),
        (maxx, miny, maxz), (maxx, maxy, maxz),        
        (maxx, maxy, maxz), (minx, maxy, maxz),        
        (minx, maxy, maxz), (minx, miny, maxz),       

        (minx, miny, minz), (minx, miny, maxz),     # Z axis
        (maxx, miny, minz), (maxx, miny, maxz),
        (maxx, maxy, minz), (maxx, maxy, maxz),
        (minx, maxy, minz), (minx, maxy, maxz)
    ], 'float32')

    indices = numpy.array([
        0, 2, 4, 6,
        8, 10, 12, 14,
        16, 18, 20, 22
    ], dtype=numpy.uint32)    

    colors = numpy.array([
        [1, 0, 0, 1],   [1, 0, 0, 1],
        [0, 0, 0, 1],   [0, 0, 0, 1],
        [0, 0, 0, 1],   [0, 0, 0, 1],
        [0, 1, 0, 1],   [0, 1, 0, 1],

        [0, 0, 0, 1],   [0, 0, 0, 1],
        [0, 0, 0, 1],   [0, 0, 0, 1],
        [0, 0, 0, 1],   [0, 0, 0, 1],
        [0, 0, 0, 1],   [0, 0, 0, 1],

        [0, 0, 1, 1],   [0, 0, 1, 1],
        [0, 0, 0, 1],   [0, 0, 0, 1],
        [0, 0, 0, 1],   [0, 0, 0, 1],
        [0, 0, 0, 1],   [0, 0, 0, 1],
    ], dtype=numpy.float32)

    bbox_geom = ospray.Geometry('curve')
    bbox_geom.set_param('vertex.position', ospray.shared_data_constructor_vec(vertices))
    bbox_geom.set_param('radius', radius)

    bbox_geom.set_param('vertex.color', ospray.shared_data_constructor_vec(colors))
    bbox_geom.set_param('index', ospray.shared_data_constructor(indices))
    bbox_geom.set_param('type', ospray.OSP_ROUND)
    bbox_geom.set_param('basis', ospray.OSP_LINEAR)
    bbox_geom.commit()

    bbox_gmodel = ospray.GeometricModel(bbox_geom)
    bbox_gmodel.commit()

    bbox_group = ospray.Group()
    bbox_group.set_param('geometry', [bbox_gmodel])
    bbox_group.commit()

    instance = ospray.Instance(bbox_group)
    instance.commit()
    instances.append(instance)
    
# User-provided scene elements

if extra_scene_script is not None:    
    
    spec = importlib.util.spec_from_file_location('extra_scene', extra_scene_script)
    mod = importlib.util.module_from_spec(spec)
    sys.modules['extra_scene'] = mod
    spec.loader.exec_module(mod)
        
    extra_instances = []
    mod.generate_instances(extra_instances)
    print('Have %d user-generated instances' % len(extra_instances))
    
    instances.extend(extra_instances)

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

if camera_configuration is None:

    camera_configuration = {'vfov': 50}

    x = minx - dimensions[0]
    y = miny - dimensions[1]*30
    z = maxz + dimensions[2]*0.6
    #position = center + 3*(bound[1] - center)
    cam_pos = camera_configuration['position'] = numpy.array([x, y, z], dtype=numpy.float32)
    cam_up = camera_configuration['up'] = numpy.array([0, 0, 1], dtype=numpy.float32)
    cam_view = camera_configuration['direction'] = center - cam_pos

    #cam_pos = numpy.array((center[0], center[1], center[2]+bound[1][2]), dtype=numpy.float32)
    #cam_pos = numpy.array((dimensions[0], dimensions[1], 2*dimensions[2]), dtype=numpy.float32)
    #cam_view = numpy.array([0, -1, 0], dtype=numpy.float32)
    #cam_up = numpy.array([0, 0, 1], dtype=numpy.float32)

    # Side
    #center = 0.5*(extent[0] + extent[1])
    #cam_pos = numpy.array([center[0], center[1]-extent[1,1], 0.5*max(extent[1])], dtype=numpy.float32)
    #cam_view = center - cam_pos
    #cam_up = numpy.array([0, 0, 1], dtype=numpy.float32)

print('Camera:', camera_configuration)

camera = ospray.Camera('perspective')
camera.set_param('aspect', W/H)
camera.set_param('fovy', camera_configuration['vfov'])
camera.set_param('position', tuple(camera_configuration['position'].tolist()))
camera.set_param('direction', tuple(camera_configuration['direction'].tolist()))
camera.set_param('up', tuple(camera_configuration['up'].tolist()))
camera.commit()

#camera = ospray.Camera('orthographic')
#camera.set_param('aspect', W/H)
#camera.set_param('height', dimensions[1]*1.1)
#camera.set_param('position', cam_pos)
#camera.set_param('direction', cam_view)
#camera.set_param('up', cam_up)
#camera.commit()

# Lights

if RENDERER == 'pathtracer' or isovalue is not None:

    light1 = ospray.Light('ambient')
    light1.set_param('color', (1.0, 1.0, 1.0))
    light1.set_param('intensity', 0.3)
    light1.commit()

    light2 = ospray.Light('distant') # sunSky
    # 2.8+ allows lights to be part of a group, and so can have a transform
    light2.set_param('direction', (0.0, 0.0, -1.0))
    light2.set_param('intensity', 0.7)
    #light2.set_param('up', (0.0, 0.0, 1.0)) # sunSky
    light2.commit()

    lights = [light1, light2]
    #lights = [light1]
    #lights = [light2]

    world.set_param('light', lights)
    world.commit()

# Renderer

#pixel = numpy.ones((1,1,3), numpy.uint8)
#backplate = ospray.Texture("texture2d")
#backplate.set_param('format', ospray.OSP_TEXTURE_RGB8)
#backplate.set_param('data', ospray.copied_data_constructor_vec(pixel))
#backplate.commit()

renderer = ospray.Renderer(RENDERER)
renderer.set_param('backgroundColor', bgcolor)

#if isovalue is not None:
#    renderer.set_param('backgroundColor', 1.0)
#else:
#    renderer.set_param('backgroundColor', (0.0, 0.0, 0.0))
#    #renderer.set_param('backplate', backplate)
if RENDERER == 'scivis':
    renderer.set_param('volumeSamplingRate', 1.0)
renderer.commit()

# Framebuffer

format = ospray.OSP_FB_SRGBA
channels = int(ospray.OSP_FB_COLOR) | int(ospray.OSP_FB_ACCUM) | int(ospray.OSP_FB_DEPTH) | int(ospray.OSP_FB_VARIANCE)

framebuffer = ospray.FrameBuffer(W, H, format, channels)
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

if display_result:
    from PIL import ImageTk
    from tkinter import *
    
    # https://stackoverflow.com/a/16536496
    root = Tk()
    tkimg = ImageTk.PhotoImage(img)
    panel = Label(root, image = tkimg)
    panel.pack(side = "bottom", fill = "both", expand = "yes")
    
    root.bind("<Escape>", lambda e: sys.exit(-1))
    root.mainloop()
