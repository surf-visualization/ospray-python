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
mesh.set_param('vertex.position', ospray.Data(vertex))
mesh.set_param('vertex.color', ospray.Data(color))
mesh.set_param('index', ospray.Data(index))
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
light.set_param('color', (1, 1, 1))
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
img.save('colors.png')
```

## Conventions and automatic data type mapping

* One Python class per OSPRay type, e.g. `Material`, `Renderer` and `Geometry`.
* Python-style method naming, so `set_param()` in Python for `setParam()` in C++
* NumPy arrays for passing arrays of numbers

When setting parameter values with `set_param()` certain Python values 
are automatically converted to OSPRay types.

| Python                    | Mapped to (C++)           | Remarks/Limitations |
| ---                       | ---                       | --- |
| 2/3/4-tuple (float, int)  | `ospcommon::math::vecXY`  | Bool not supported yet |
| NumPy array               | `ospray::cpp::Data`       | Only for 1 and 2 dimensional arrays of floats |
| List of OSPRay objects    | `ospray::cpp::Data`       | Only for GeometricModel, Instance, Light |

Notes:

- Passing regular lists of Python numbers is not supported. Use
  NumPy arrays for those cases.

Examples:

```
# Tuple -> vec3f
light = ospray.Light('ambient')
light.set_param('color', (1, 1, 1))

# NumPy array to ospray::cpp::Data
index = numpy.array([
    [0, 1, 2], [1, 2, 3]
], dtype=numpy.uint32)
mesh = ospray.Geometry('mesh')
mesh.set_param('index', index)

# List of objects to ospray::cpp::Data
world.set_param('light', [light1,light2])
```
