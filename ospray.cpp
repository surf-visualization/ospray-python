#include <pybind11/pybind11.h>
#include <pybind11/numpy.h>
#include <pybind11/stl.h>
#include "ospray/ospray_cpp.h"

namespace py = pybind11;

OSPError
init(const std::vector<std::string>& args)
{
    int argc = args.size();
    const char **argv = new const char*[argc];
    
    for (int i = 0; i < argc; i++)
        argv[i] = args[i].c_str();
    
    OSPError res = ospInit(&argc, argv);
    
    // XXX return modified array?
    delete [] argv;
    
    //if (res != OSP_NO_ERROR)
    //    throw std::exception("ospInit() failed");
    
    return res;
}

template<typename T>
void
set_param_bool(T &self, const std::string &name, const bool &value)
{
    self.setParam(name, value);
}

template<typename T>
void
set_param_float(T &self, const std::string &name, const float &value)
{
    self.setParam(name, value);
}

template<typename T>
void
set_param_int(T &self, const std::string &name, const int &value)
{
    self.setParam(name, value);
}

template<typename T>
void
set_param_data(T &self, const std::string &name, const ospray::cpp::Data &data)
{
    self.setParam(name, data);
}

template<typename T>
void
set_param_tuple(T &self, const std::string &name, const py::tuple &value)
{
    auto n = value.size();
    
    if (n == 2)
    {
        if (py::isinstance<py::int_>(value[0]))
        {
            ospcommon::math::vec2i vvalue;
            
            vvalue.x = py::cast<int>(value[0]);
            vvalue.y = py::cast<int>(value[1]);
            
            self.setParam(name, vvalue);
        }
        else if (py::isinstance<py::float_>(value[0]))
        {
            ospcommon::math::vec2f vvalue;
            
            vvalue.x = py::cast<float>(value[0]);
            vvalue.y = py::cast<float>(value[1]);
            
            self.setParam(name, vvalue);
        }
        else
            printf("WARNING: unhandled data type in set_param_tuple!\n");
    }
    
    else if (n == 3)
    {
        if (py::isinstance<py::int_>(value[0]))
        {
            ospcommon::math::vec3i vvalue;
            
            vvalue.x = py::cast<int>(value[0]);
            vvalue.y = py::cast<int>(value[1]);
            vvalue.z = py::cast<int>(value[2]);
            
            self.setParam(name, vvalue);
        }
        else if (py::isinstance<py::float_>(value[0]))
        {
            ospcommon::math::vec3f vvalue;
            
            vvalue.x = py::cast<float>(value[0]);
            vvalue.y = py::cast<float>(value[1]);
            vvalue.z = py::cast<float>(value[2]);
            
            self.setParam(name, vvalue);
        }
        else
            printf("WARNING: unhandled data type in set_param_tuple!\n");
    }
    
    else if (n == 4)
    {
        if (py::isinstance<py::int_>(value[0]))
        {
            ospcommon::math::vec4i vvalue;
            
            vvalue.x = py::cast<int>(value[0]);
            vvalue.y = py::cast<int>(value[1]);
            vvalue.z = py::cast<int>(value[2]);
            vvalue.w = py::cast<int>(value[3]);
            
            self.setParam(name, vvalue);
        }
        else if (py::isinstance<py::float_>(value[0]))
        {
            ospcommon::math::vec4f vvalue;
            
            vvalue.x = py::cast<float>(value[0]);
            vvalue.y = py::cast<float>(value[1]);
            vvalue.z = py::cast<float>(value[2]);
            vvalue.w = py::cast<float>(value[3]);
            
            self.setParam(name, vvalue);
        }
        else
            printf("WARNING: unhandled data type in set_param_tuple!\n");
    }
    
    else
        printf("WARNING: unhandled tuple length %lu set_param_tuple!\n", n);
}

template<typename T>
void
set_param_material(T& self, const std::string &name, const ospray::cpp::Material &value)
{
    self.setParam(name, value);
}

template<typename T>
void
set_param_numpy_array(T &self, const std::string &name, py::array_t<float, py::array::c_style | py::array::forcecast> array)
{
    const int ndim = array.ndim();
    const py::dtype& dtype = array.dtype();
    
    if (ndim == 1)
    {
        const int n = array.shape(0);
        
        if (dtype.is(pybind11::dtype::of<float>()) && n == 3)
        {
            const ospcommon::math::vec3f varray(array.data());
            self.setParam(name, varray);
        }
        else if (dtype.is(pybind11::dtype::of<float>()) && n == 4)
        {
            const ospcommon::math::vec4f varray(array.data());
            self.setParam(name, varray);
        }
    }
    else if (ndim == 2)
    {
        const int n = array.shape(0);
        const int p = array.shape(1);
        
        if (dtype.is(pybind11::dtype::of<float>()) && p == 3)
        {
            self.setParam(name, ospray::cpp::Data(n*p, array.data()));
        }
    }
}

ospray::cpp::Data
data_constructor(py::array& array)
{
    const int ndim = array.ndim();
    const py::dtype& dtype = array.dtype();
    
    ospcommon::math::vec3ul num_items { 1, 1, 1 };
    ospcommon::math::vec3ul byte_stride { 0, 0, 0 };
    
    num_items.x = array.shape(0);
    
    if (ndim == 1)
    {
        if (dtype.is(pybind11::dtype::of<float>()))
            return ospray::cpp::Data(num_items, byte_stride, (float*)array.data());
        else if (dtype.is(pybind11::dtype::of<int>()))
            return ospray::cpp::Data(num_items, byte_stride, (int*)array.data());
        else if (dtype.is(pybind11::dtype::of<uint8_t>()))
            return ospray::cpp::Data(num_items, byte_stride, (uint8_t*)array.data());
        else if (dtype.is(pybind11::dtype::of<uint32_t>()))
            return ospray::cpp::Data(num_items, byte_stride, (uint32_t*)array.data());
        else if (dtype.is(pybind11::dtype::of<uint64_t>()))
            return ospray::cpp::Data(num_items, byte_stride, (uint64_t*)array.data());
    }
    else if (ndim == 2)
    {
        if (array.shape(1) == 2)
        {
            if (dtype.is(pybind11::dtype::of<float>()))
                return ospray::cpp::Data(num_items, byte_stride, (ospcommon::math::vec2f*)array.data());
            else if (dtype.is(pybind11::dtype::of<int>()))
                return ospray::cpp::Data(num_items, byte_stride, (ospcommon::math::vec2i*)array.data());
            else if (dtype.is(pybind11::dtype::of<uint32_t>()))
                return ospray::cpp::Data(num_items, byte_stride, (ospcommon::math::vec2ui*)array.data());
        }       
        else if (array.shape(1) == 3)
        {
            if (dtype.is(pybind11::dtype::of<float>()))
                return ospray::cpp::Data(num_items, byte_stride, (ospcommon::math::vec3f*)array.data());
            else if (dtype.is(pybind11::dtype::of<int>()))
                return ospray::cpp::Data(num_items, byte_stride, (ospcommon::math::vec3i*)array.data());
            else if (dtype.is(pybind11::dtype::of<uint32_t>()))
                return ospray::cpp::Data(num_items, byte_stride, (ospcommon::math::vec3ui*)array.data());
        }
        else if (array.shape(1) == 4)
        {
            if (dtype.is(pybind11::dtype::of<float>()))
                return ospray::cpp::Data(num_items, byte_stride, (ospcommon::math::vec4f*)array.data());
            else if (dtype.is(pybind11::dtype::of<int>()))
                return ospray::cpp::Data(num_items, byte_stride, (ospcommon::math::vec4i*)array.data());
            else if (dtype.is(pybind11::dtype::of<uint32_t>()))
                return ospray::cpp::Data(num_items, byte_stride, (ospcommon::math::vec4ui*)array.data());
        }
    }
     
    printf("WARNING: unhandled data type in data_constructor(), ndim %d, kind '%c'!\n", ndim, dtype.kind());
    
    return ospray::cpp::Data();
}

ospray::cpp::FrameBuffer
framebuffer_create(py::tuple& imgsize, OSPFrameBufferFormat format, int channels)
{
    int w = py::cast<int>(imgsize[0]);
    int h = py::cast<int>(imgsize[1]);
    ospcommon::math::vec2i isize { w, h };
    
    return ospray::cpp::FrameBuffer(isize, format, channels);
}

py::array
framebuffer_get(ospray::cpp::FrameBuffer &self, OSPFrameBufferChannel channel, py::tuple& imgsize, OSPFrameBufferFormat format)
{
    int w = py::cast<int>(imgsize[0]);
    int h = py::cast<int>(imgsize[1]);
    py::array res;
    void *fb;
    
    fb = self.map(channel);
    
    if (channel == OSP_FB_COLOR)
    {
        if (format == OSP_FB_SRGBA || format == OSP_FB_RGBA8)
            res = py::array_t<uint8_t>({w,h,4}, (uint8_t*)fb);
        else if (format == OSP_FB_RGBA32F)
            res = py::array_t<float>({w,h,4}, (float*)fb);
    }
    else if (channel == OSP_FB_DEPTH)
    {
        res = py::array_t<float>({w,h}, (float*)fb);
    }
    
    self.unmap(fb);
    
    return res;
}

template<typename T>
py::tuple
get_bounds(T &self)
{
    ospcommon::math::box3f bounds = self.getBounds();
    
    return py::make_tuple(
        bounds.lower.x, bounds.lower.y, bounds.lower.z,
        bounds.upper.x, bounds.upper.y, bounds.upper.z
    );
}

template<typename T>
void
declare_managedobject_methods(py::module& m, const char *name)
{
    py::class_<T>(m, name)
        //(void (T::*)(const std::string &, const float &)) 
        .def("set_param", &set_param_bool<T>)
        .def("set_param", &set_param_int<T>)
        .def("set_param", &set_param_float<T>)
        .def("set_param", &set_param_data<T>)
        .def("set_param", &set_param_numpy_array<T>)
        .def("set_param", &set_param_tuple<T>)
        .def("set_param", &set_param_material<T>)
        .def("commit", &T::commit)
        .def("get_bounds", &get_bounds<T>)
    ;
}

typedef ospray::cpp::ManagedObject<OSPCamera, OSP_CAMERA>                   ManagedCamera;
typedef ospray::cpp::ManagedObject<OSPData, OSP_DATA>                       ManagedData;
typedef ospray::cpp::ManagedObject<OSPFrameBuffer, OSP_FRAMEBUFFER>         ManagedFrameBuffer;
typedef ospray::cpp::ManagedObject<OSPFuture, OSP_FUTURE>                   ManagedFuture;
typedef ospray::cpp::ManagedObject<OSPGeometricModel, OSP_GEOMETRIC_MODEL>  ManagedGeometricModel;
typedef ospray::cpp::ManagedObject<OSPGeometry, OSP_GEOMETRY>               ManagedGeometry;
typedef ospray::cpp::ManagedObject<OSPGroup, OSP_GROUP>                     ManagedGroup;
typedef ospray::cpp::ManagedObject<OSPInstance, OSP_INSTANCE>               ManagedInstance;
typedef ospray::cpp::ManagedObject<OSPLight, OSP_LIGHT>                     ManagedLight;
typedef ospray::cpp::ManagedObject<OSPMaterial, OSP_MATERIAL>               ManagedMaterial;
typedef ospray::cpp::ManagedObject<OSPRenderer, OSP_RENDERER>               ManagedRenderer;
typedef ospray::cpp::ManagedObject<OSPWorld, OSP_WORLD>                     ManagedWorld;

PYBIND11_MODULE(ospray, m) 
{
    m.doc() = "ospray bindings";
    
    py::enum_<OSPError>(m, "OSPError")
        .value("OSP_NO_ERROR", OSPError::OSP_NO_ERROR)
        .value("OSP_UNKNOWN_ERROR", OSPError::OSP_UNKNOWN_ERROR)
        .value("OSP_INVALID_ARGUMENT", OSPError::OSP_INVALID_ARGUMENT)
        .value("OSP_INVALID_OPERATION", OSPError::OSP_INVALID_OPERATION)
        .value("OSP_OUT_OF_MEMORY", OSPError::OSP_OUT_OF_MEMORY)
        .value("OSP_UNSUPPORTED_CPU", OSPError::OSP_UNSUPPORTED_CPU)
        .value("OSP_VERSION_MISMATCH", OSPError::OSP_VERSION_MISMATCH)
        .export_values()
    ;
    
    py::enum_<OSPFrameBufferFormat>(m, "OSPFrameBufferFormat")
        .value("OSP_FB_NONE", OSPFrameBufferFormat::OSP_FB_NONE)
        .value("OSP_FB_RGBA8", OSPFrameBufferFormat::OSP_FB_RGBA8)
        .value("OSP_FB_SRGBA", OSPFrameBufferFormat::OSP_FB_SRGBA)
        .value("OSP_FB_RGBA32F", OSPFrameBufferFormat::OSP_FB_RGBA32F)
        .export_values()
    ;
    
    py::enum_<OSPFrameBufferChannel>(m, "OSPFrameBufferChannel")
        .value("OSP_FB_COLOR", OSPFrameBufferChannel::OSP_FB_COLOR)
        .value("OSP_FB_DEPTH", OSPFrameBufferChannel::OSP_FB_DEPTH)
        .value("OSP_FB_ACCUM", OSPFrameBufferChannel::OSP_FB_ACCUM)
        .value("OSP_FB_VARIANCE", OSPFrameBufferChannel::OSP_FB_VARIANCE)
        .value("OSP_FB_NORMAL", OSPFrameBufferChannel::OSP_FB_NORMAL)
        .value("OSP_FB_ALBEDO", OSPFrameBufferChannel::OSP_FB_ALBEDO)
        .export_values()
    ;
    
    py::enum_<OSPSyncEvent>(m, "OSPSyncEvent")
        .value("OSP_NONE_FINISHED", OSPSyncEvent::OSP_NONE_FINISHED)
        .value("OSP_WORLD_RENDERED", OSPSyncEvent::OSP_WORLD_RENDERED)
        .value("OSP_WORLD_COMMITTED", OSPSyncEvent::OSP_WORLD_COMMITTED)
        .value("OSP_FRAME_FINISHED", OSPSyncEvent::OSP_FRAME_FINISHED)
        .value("OSP_TASK_FINISHED", OSPSyncEvent::OSP_TASK_FINISHED)
        .export_values()
    ;
    
    m.def("init", &init);
        
    declare_managedobject_methods<ManagedCamera>(m, "ManagedCamera");
    declare_managedobject_methods<ManagedData>(m, "ManagedData");
    declare_managedobject_methods<ManagedFrameBuffer>(m, "ManagedFrameBuffer");
    declare_managedobject_methods<ManagedFuture>(m, "ManagedFuture");
    declare_managedobject_methods<ManagedGeometricModel>(m, "ManagedGeometricModel");
    declare_managedobject_methods<ManagedGeometry>(m, "ManagedGeometry");
    declare_managedobject_methods<ManagedGroup>(m, "ManagedGroup");
    declare_managedobject_methods<ManagedInstance>(m, "ManagedInstance");
    declare_managedobject_methods<ManagedLight>(m, "ManagedLight");
    declare_managedobject_methods<ManagedMaterial>(m, "ManagedMaterial");
    declare_managedobject_methods<ManagedRenderer>(m, "ManagedRenderer");
    declare_managedobject_methods<ManagedWorld>(m, "ManagedWorld");
    
    py::class_<ospray::cpp::Camera, ManagedCamera >(m, "Camera")
        .def(py::init<const std::string &>())
    ;

    py::class_<ospray::cpp::Data, ManagedData >(m, "Data")
        .def(py::init<const ospray::cpp::GeometricModel &>())
        .def(py::init<const ospray::cpp::Geometry &>())
        .def(py::init<const ospray::cpp::Instance &>())
        .def(py::init<const ospray::cpp::Light &>())
        .def(py::init(
            [](py::array& array) {
                return data_constructor(array);
            }))
    ;
            
    py::class_<ospray::cpp::FrameBuffer, ManagedFrameBuffer >(m, "FrameBuffer")
        .def(py::init(
            [](py::tuple& imgsize, OSPFrameBufferFormat format=OSP_FB_SRGBA, int channels=OSP_FB_COLOR) {
                return framebuffer_create(imgsize, format, channels);
            }))
        .def("clear", &ospray::cpp::FrameBuffer::clear)
        //.def("get_variance", &ospray::cpp::FrameBuffer::getVariance)  // XXX doesn't exist
        .def("get", &framebuffer_get)
        //.def("map", &ospray::cpp::FrameBuffer::map)
        .def("render_frame", &ospray::cpp::FrameBuffer::renderFrame)
        .def("reset_accumulation", &ospray::cpp::FrameBuffer::resetAccumulation)
        //.def("unmap", &ospray::cpp::FrameBuffer::unmap)
    ;
       
    py::class_<ospray::cpp::Future, ManagedFuture >(m, "Future")
        .def(py::init<>())
        .def("is_ready", &ospray::cpp::Future::isReady, py::arg("event")=OSP_TASK_FINISHED)
        .def("wait", &ospray::cpp::Future::wait, py::arg("event")=OSP_TASK_FINISHED)
        .def("cancel", &ospray::cpp::Future::cancel)
        .def("progress", &ospray::cpp::Future::progress)
    ;            
            
    py::class_<ospray::cpp::GeometricModel, ManagedGeometricModel >(m, "GeometricModel")
        .def(py::init<const ospray::cpp::Geometry &>())
    ;
    
    py::class_<ospray::cpp::Geometry, ManagedGeometry >(m, "Geometry")
        .def(py::init<const std::string &>())
    ;
    
    py::class_<ospray::cpp::Group, ManagedGroup>(m, "Group")
        .def(py::init<>())
    ;
            
    py::class_<ospray::cpp::Instance, ManagedInstance>(m, "Instance")
        .def(py::init<ospray::cpp::Group &>())
    ;
            
    py::class_<ospray::cpp::Light, ManagedLight>(m, "Light")
        .def(py::init<const std::string &>())
    ;
    
    py::class_<ospray::cpp::Material, ManagedMaterial>(m, "Material")
        .def(py::init<const std::string &, const std::string &>())
    ;
    
    py::class_<ospray::cpp::Renderer, ManagedRenderer>(m, "Renderer")
        .def(py::init<const std::string &>())
    ;
    
    py::class_<ospray::cpp::World, ManagedWorld >(m, "World")
        .def(py::init<>())
    ;

}