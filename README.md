# OSPY - Python bindings for OSPRay

These are experimental Python 3.x bindings for [OSPRay](https://www.ospray.org).
Originally just to try out [pybind11](https://github.com/pybind/pybind11),
but they are pretty useful in their current state, as Pybind11 is an amazing little
library (inpsired by the just-as-great Boost.Python).

Note that this code is targeted at the 2.0.x branch of OSPRay.

## Example (after ospTutorial.cpp)

```
#!/usr/bin/env python
import sys
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
mesh.set_param('vertex.position', vertex)
mesh.set_param('vertex.color', color)
mesh.set_param('index', index)
mesh.commit()

gmodel = ospray.GeometricModel(mesh)
gmodel.commit()

group = ospray.Group()
group.set_param('geometry', [gmodel])
group.commit()

instance = ospray.Instance(group)
instance.commit()

material = ospray.Material('pathtracer', 'OBJMaterial')
material.commit()

gmodel.set_param('material', material)
gmodel.commit()

world = ospray.World()
world.set_param('instance', [instance])

light = ospray.Light('ambient')
light.commit()

world.set_param('light', [light])
world.commit()

renderer = ospray.Renderer('pathtracer')
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

img = Image.frombuffer('RGBA', (W,H), colors, 'raw', 'RGBA', 0, 1)
img = img.transpose(Image.FLIP_TOP_BOTTOM)
img.save('colors.png')
```

## Conventions 

- One Python class per OSPRay type, e.g. `Material`, `Renderer` and `Geometry`.
- Python-style method naming, so `set_param()` in Python for `setParam()` in C++
- NumPy arrays for passing larger arrays of numbers, tuples for small single-dimensional
  values such as `vec3f`'s.
- Framebuffer `map()`/`unmap()` is not wrapped, but instead a method `get(channel, imgsize, format)`
  is available that directly provides the requested framebuffer channel as
  a NumPy array. Note that the pixel order is the same as what `FrameBuffer.map()`
  returns: the first pixel returned is at the lower left pixel of the image.

## Automatic data type mapping

When setting parameter values with `set_param()` certain Python values 
are automatically converted to OSPRay types:

- A 2/3/4-tuple of float or int is converted to a corresponding 
  `ospcommon::math::vec<n>[i|f]` value. If there is at least one float
  in a tuple it is converted to a `vec<n>f` value, otherwise a `vec<n>i`
  value is produced.
  
- A single-dimensional NumPy array is converted to a `ospray::cpp::Data` value
  of the corresponding C++ type. E.g. a NumPy array of N `numpy.uint32` values 
  is converted to a `Data` object of type `uint32_t`.
  
- Two-dimensional NumPy arrays are converted to `ospray::cpp::Data` values
  of the corresponding *vector* type, based on the second dimension of the array.
  E.g. a NumPy array of shape (N,3) of `numpy.float32` is converted to a `Data` object
  of `ospcommon::math::vec3f` values. 
  
  An exception here is a NumPy array of shape (N, 1). This is converted
  to a single-dimensional `Data` object of N values.
  
- Three-dimensional NumPy arrays are converted to `ospray::cpp::Data` values
  of the corresponding type and dimensions. 
  
- Lists of OSPRay objects are turned into a `Data` object. The list items
  must all have the same type and are currently limited to GeometricModel, 
  Instance, Material and VolumetricModel. 

- Passing regular lists of Python numbers is not supported. Use
  NumPy arrays for those cases.

Examples:

```
# 3-tuple -> vec3f
light1 = ospray.Light('ambient')
light1.set_param('color', (1, 1, 1))

# NumPy array to ospray::cpp::Data
index = numpy.array([
    [0, 1, 2], [1, 2, 3]
], dtype=numpy.uint32)
mesh = ospray.Geometry('mesh')
mesh.set_param('index', index)

camera = ospray.Camera('perspective')
# Tuple of floats -> ospcommon::math::vec3f
camera.set_param('up', (0.0, 0.0, 1.0))
# NumPy array to ospcommon::math::vec3f isn't directly possible.
# Use manual conversion to tuple, or set a tuple directly
cam_pos = numpy.array([1, 2, 3.5], dtype=numpy.float32)
camera.set_param('position', tuple(cam_pos.tolist()))

# List of scene objects to ospray::cpp::Data
world.set_param('light', [light1,light2])
```
