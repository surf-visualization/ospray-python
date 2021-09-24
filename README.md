# ospray-python - Python bindings for OSPRay

These are Python 3.x bindings for [OSPRay](https://www.ospray.org).
More specifically, they wrap the OSPRay C++ API (plus a few C API routines).
Most OSPRay objects can be created from Python. Due to the general
method of setting object parameters in OSPRay these are supported
on all objects as well. But see the notes on data type mapping
below.

Note that this code is targeted at the 2.7.x branch of OSPRay.

These bindings started just to try out [pybind11](https://github.com/pybind/pybind11),
but they have quickly become pretty useful, as Pybind11 is an amazing little
library (inspired by the just-as-great Boost.Python) that makes wrapping
C++ very easy.

## Conventions 

- One Python class per OSPRay type, e.g. `Material`, `Renderer` and `Geometry`
- Python-style method naming, so `set_param()` in Python for `setParam()` in C++
- NumPy arrays for passing larger arrays of numbers, Python tuples for small 
  single vector values such as `vec3f`'s
- Framebuffer `map()`/`unmap()` is not wrapped, but instead a method `get(channel, imgsize, format)`
  is available that directly provides the requested framebuffer channel as
  a NumPy array. Note that the pixel order is the same as what `FrameBuffer.map()`
  returns: the first pixel returned is at the lower-left of the image.

## Data type mapping

For passing arrays of numbers, such as vertex coordinates or lists of isosurface
values, use NumPy arrays. These can be converted to a `Data` object using the 
`data_constructor()` or `data_constructor_vec()` functions. The `data_constructor()`
function directly converts the NumPy array to a `Data` array of the same dimensions
and data type. 

The `data_constructor_vec()` function can be used for generating `Data` arrays of 
`vec<n><t>` values. The last dimension of the passed array must 
be 2, 3 or 4. 

When setting parameter values with `set_param()` certain Python values 
are automatically converted to OSPRay types:

- A 2/3/4-tuple of float or int is converted to a corresponding 
  `vec<n>[i|f]` value. If there is at least one float
  in the tuple it is converted to a `vec<n>f` value, otherwise a `vec<n>i`
  value is produced.
  
- A NumPy array is converted to a `Data` array using `data_constructor()`, so
  you can pass Numpy arrays directly to `set_param()` without having to call
  `data_constructor()` yourself. However, for parameters that expect vector 
  values (i.e. `vec<n><t>`) an explicit conversion using `data_constructor_vec()` 
  is needed.
  
- Lists of OSPRay objects are turned into a `Data` object. The list items
  must all have the same type and are currently limited to GeometricModel, 
  ImageOperation, Instance, Material and VolumetricModel. 

- Passing regular Python lists of numbers is not supported. Use NumPy 
  arrays instead.

- OSPRay currently does not support all integer types for `Data` objects 
  (e.g. not `int8`), nor all combinations of vector length and data type 
  (e.g. not `vec2d`).

Both data constructor functions take a second argument `is_shared` defaulting
to `False` that determines if the data in the NumPy array passed can simply 
be referenced instead of copied. In case `is_shared` is `True` you need to 
make sure the NumPy array stays alive by referencing it somewhere in your
program, as the data in the NumPy array will be used by the `Data` object
directly.

Examples:

```
# 3-tuple -> vec3f
light1 = ospray.Light('ambient')
light1.set_param('color', (1.0, 1, 1))

# NumPy array to ospray::cpp::Data of vec3ui values
index = numpy.array([
    [0, 1, 2], [1, 2, 3]
], dtype=numpy.uint32)
mesh = ospray.Geometry('mesh')
mesh.set_param('index', ospray.data_constructor_vec(index))

camera = ospray.Camera('perspective')
# Tuple of floats -> vec3f
camera.set_param('up', (0.0, 0.0, 1.0))
# NumPy array to ospcommon::math::vec3f isn't directly possible.
# Use manual conversion to tuple, or set a tuple directly
cam_pos = numpy.array([1, 2, 3.5], dtype=numpy.float32)
camera.set_param('position', tuple(cam_pos.tolist()))

# List of scene objects to ospray::cpp::Data
world.set_param('light', [light1,light2])
```

## Context manager for automatic object commit

Most objects (except `Data` and `Device`) support using them as a
context manager together with the `with` statement. On exit of the
`with` block `commit()` is automatically called on the object. 

So instead of

```
light = ospray.Light('ambient')
light.set_param('color', (1.0, 1, 1))
light.set_param('intensity', 1.0)
# It's easy to forget the call to commit()
light.commit()
```

you can write

```
light = ospray.Light('ambient')
with light:
    light.set_param('color', (1.0, 1, 1))
    light.set_param('intensity', 1.0)
    # commit() will be called automatically at this point
```

Or with the new [assignment expression](https://docs.python.org/3/whatsnew/3.8.html#assignment-expressions)
introduced in Python 3.8 this can even be shortened to

```
with (light := ospray.Light('ambient')):
    light.set_param('color', (1.0, 1, 1))
    light.set_param('intensity', 1.0)
```

# Missing features and/or limitations

- Not all mathematical operations on `affine3f` are supported. Affine values of other sizes and data types are not included.
- No way to specify `Data` strides
- No subdivision surface edge boundary enums (and probably some other enums as well)
- Not all `Device` methods are available
- Could throw more exceptions in cases where currently a warning/error is printed

# Example (after ospTutorial.cpp)

```
#!/usr/bin/env python
import sys, os

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
mesh.set_param('vertex.position', ospray.data_constructor_vec(vertex))
mesh.set_param('vertex.color', ospray.data_constructor_vec(color))
mesh.set_param('index', ospray.data_constructor_vec(index))
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

framebuffer = ospray.FrameBuffer((W,H), format, channels)
framebuffer.clear()

for frame in range(8):
    future = framebuffer.render_frame(renderer, camera, world)
    future.wait()
    print('[%d] %.6f' % (frame, framebuffer.get_variance()))
    
res = framebuffer.pick(renderer, camera, world, (0.5, 0.5))
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
```
