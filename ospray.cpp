#include <pybind11/pybind11.h>
#include <pybind11/numpy.h>
#include <pybind11/stl.h>
#include "ospray/ospray_cpp.h"

namespace py = pybind11;

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

static py::function py_error_func;
static py::function py_status_func;

static void
error_func(OSPError error, const char *details)
{
    if (py_error_func)
        py_error_func(error, details);
}

static void
status_func(const char *message)
{
    if (py_status_func)
        py_status_func(message);
}

std::vector<std::string>
init(const std::vector<std::string>& args)
{
    int argc = args.size();
    const char **argv = new const char*[argc];
    
    for (int i = 0; i < argc; i++)
        argv[i] = args[i].c_str();
    
    OSPError res = ospInit(&argc, argv);
    
    if (res != OSP_NO_ERROR)
    {
        delete [] argv;
        std::string msg = "ospInit() failed: " + std::to_string(res);
        throw std::runtime_error(msg);
    }
    
    std::vector<std::string> newargs;
    
    for (int i = 0; i < argc; i++)
        newargs.push_back(std::string(argv[i]));
    
    delete [] argv;
    
    // Set C++ wrappers for error and status callbacks
    
    OSPDevice device = ospGetCurrentDevice();
    ospDeviceSetErrorFunc(device, error_func);
    ospDeviceSetStatusFunc(device, status_func);
    
    return newargs;
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
        else if (dtype.is(pybind11::dtype::of<double>()))
        {
            printf("WARNING: passing Data object of double, are you sure that's what you want?\n");
            return ospray::cpp::Data(num_items, byte_stride, (double*)array.data());
        }
        else if (dtype.is(pybind11::dtype::of<int>()))
            return ospray::cpp::Data(num_items, byte_stride, (int*)array.data());
        else if (dtype.is(pybind11::dtype::of<uint8_t>()))
            return ospray::cpp::Data(num_items, byte_stride, (uint8_t*)array.data());
        else if (dtype.is(pybind11::dtype::of<uint32_t>()))
            return ospray::cpp::Data(num_items, byte_stride, (uint32_t*)array.data());
        else if (dtype.is(pybind11::dtype::of<uint64_t>()))
            return ospray::cpp::Data(num_items, byte_stride, (uint64_t*)array.data());
        
        printf("WARNING: unhandled data type in data_constructor(), ndim 1, shape=%ld, kind '%c'!\n", 
            array.shape(0), dtype.kind());
        
        return ospray::cpp::Data();
    }
    else if (ndim == 2)
    {
        const int w = array.shape(1);
        
        if (dtype.is(pybind11::dtype::of<double>()))
        {
            printf("WARNING: attempt to pass double precision vector values, but only single-precision (float) is available\n");
            return ospray::cpp::Data();
        }
        
        if (w == 2)
        {
            if (dtype.is(pybind11::dtype::of<float>()))
                return ospray::cpp::Data(num_items, byte_stride, (ospcommon::math::vec2f*)array.data());
            else if (dtype.is(pybind11::dtype::of<int>()))
                return ospray::cpp::Data(num_items, byte_stride, (ospcommon::math::vec2i*)array.data());
            else if (dtype.is(pybind11::dtype::of<uint32_t>()))
                return ospray::cpp::Data(num_items, byte_stride, (ospcommon::math::vec2ui*)array.data());
        }       
        else if (w == 3)
        {
            if (dtype.is(pybind11::dtype::of<float>()))
                return ospray::cpp::Data(num_items, byte_stride, (ospcommon::math::vec3f*)array.data());
            else if (dtype.is(pybind11::dtype::of<int>()))
                return ospray::cpp::Data(num_items, byte_stride, (ospcommon::math::vec3i*)array.data());
            else if (dtype.is(pybind11::dtype::of<uint32_t>()))
                return ospray::cpp::Data(num_items, byte_stride, (ospcommon::math::vec3ui*)array.data());
        }
        else if (w == 4)
        {
            if (dtype.is(pybind11::dtype::of<float>()))
                return ospray::cpp::Data(num_items, byte_stride, (ospcommon::math::vec4f*)array.data());
            else if (dtype.is(pybind11::dtype::of<int>()))
                return ospray::cpp::Data(num_items, byte_stride, (ospcommon::math::vec4i*)array.data());
            else if (dtype.is(pybind11::dtype::of<uint32_t>()))
                return ospray::cpp::Data(num_items, byte_stride, (ospcommon::math::vec4ui*)array.data());
        }

        printf("WARNING: unhandled data type in data_constructor(), ndim 2, shape=(%ld,%ld), kind '%c'!\n", 
            array.shape(0), array.shape(1), dtype.kind());
        
        return ospray::cpp::Data();
    }
     
    printf("WARNING: unhandled data type in data_constructor(), ndim %d, kind '%c'!\n", ndim, dtype.kind());
    
    return ospray::cpp::Data();
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
    
    if (n < 2 || n > 4)
    {
        printf("ERROR: tuple length for '%s' should be in range [2,4]!\n", name.c_str());
        return;
    }

    auto first = value[0];
    std::string firstcls = first.get_type().attr("__name__").cast<std::string>();
    
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
            printf("WARNING: unhandled data type (%s) in set_param_tuple(..., '%s', ...)!\n", firstcls.c_str(), name.c_str());
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
            printf("WARNING: unhandled data type (%s) in set_param_tuple(..., '%s', ...)!\n", firstcls.c_str(), name.c_str());
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
            printf("WARNING: unhandled data type (%s) in set_param_tuple(..., '%s', ...)!\n", firstcls.c_str(), name.c_str());
    }
}

template<typename T>
ospray::cpp::Data
build_data_list(const std::string& listcls, const py::list &values)
{
    std::vector<T> items;
        
    for (size_t i = 0; i < values.size(); i++)
    {
        auto item = values[i];
        std::string itemcls = item.get_type().attr("__name__").cast<std::string>();
        
        if (itemcls != listcls)
        {
            printf("ERROR: item %lu in list is not of type %s, but of type %s!\n", 
                i, listcls.c_str(), itemcls.c_str());
            
            return ospray::cpp::Data();
        }
        
        items.push_back(values[i].cast<T>());
    }
    
    return ospray::cpp::Data(items);
}

// List assumed to only contain OSPObject's of the same type
template<typename T>
void
set_param_list(T &self, const std::string &name, const py::list &values)
{
    auto first = values[0];
    
    std::string listcls = first.get_type().attr("__name__").cast<std::string>();
    
    printf("%s\n", listcls.c_str());
    
    if (listcls == "GeometricModel")
        self.setParam(name, build_data_list<ospray::cpp::GeometricModel>(listcls, values));
    else if (listcls == "Instance")
        self.setParam(name, build_data_list<ospray::cpp::Instance>(listcls, values));
    else if (listcls == "Light")
        self.setParam(name, build_data_list<ospray::cpp::Light>(listcls, values));
    else
        printf("WARNING: unhandled list with items of type %s in set_param_list()!\n", listcls.c_str());
}

template<typename T>
void
set_param_material(T& self, const std::string &name, const ospray::cpp::Material &value)
{
    self.setParam(name, value);
}

template<typename T>
void
set_param_numpy_array(T &self, const std::string &name, py::array& array)
{
    const int ndim = array.ndim();
    const int n = array.shape(0);
    
    if (ndim > 1 || n < 2 || n > 4)
    {
        self.setParam(name, data_constructor(array));
        return;
    }
    
    const py::dtype& dtype = array.dtype();
    
    if (dtype.is(pybind11::dtype::of<float>()))
    {
        const float *values = (float*)(array.data());
        
        if (n == 2)
            self.setParam(name, ospcommon::math::vec2f(values));
        else if (n == 3)
            self.setParam(name, ospcommon::math::vec3f(values));
        else if (n == 4)
            self.setParam(name, ospcommon::math::vec4f(values));
    }
    else if (dtype.is(pybind11::dtype::of<int>()))
    {
        const int *values = (int*)(array.data()); 
        
        if (n == 2)
            self.setParam(name, ospcommon::math::vec2i(values));
        else if (n == 3)
            self.setParam(name, ospcommon::math::vec3i(values));
        else if (n == 4)
            self.setParam(name, ospcommon::math::vec4i(values));
    }
    else if (dtype.is(pybind11::dtype::of<uint32_t>()))
    {
        const uint32_t *values = (uint32_t*)(array.data()); 
        
        if (n == 2)
            self.setParam(name, ospcommon::math::vec2ui(values));
        else if (n == 3)
            self.setParam(name, ospcommon::math::vec3ui(values));
        else if (n == 4)
            self.setParam(name, ospcommon::math::vec4ui(values));
    }
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
        .def("set_param", &set_param_tuple<T>)
        .def("set_param", &set_param_list<T>)
        .def("set_param", &set_param_data<T>)
        .def("set_param", &set_param_numpy_array<T>)
        .def("set_param", &set_param_material<T>)
        .def("commit", &T::commit)
        .def("get_bounds", &get_bounds<T>)
    ;
}

PYBIND11_MODULE(ospray, m) 
{
    m.doc() = "OSPRay bindings";
    
    py::enum_<OSPDataType>(m, "OSPDataType")
        .value("OSP_DEVICE", OSPDataType::OSP_DEVICE)
        .value("OSP_VOID_PTR", OSPDataType::OSP_VOID_PTR)
        .value("OSP_BOOL", OSPDataType::OSP_BOOL)
        .value("OSP_OBJECT", OSPDataType::OSP_OBJECT)
        .value("OSP_DATA", OSPDataType::OSP_DATA)
        .value("OSP_CAMERA", OSPDataType::OSP_CAMERA)
        .value("OSP_FRAMEBUFFER", OSPDataType::OSP_FRAMEBUFFER)
        .value("OSP_FUTURE", OSPDataType::OSP_FUTURE)
        .value("OSP_GEOMETRIC_MODEL", OSPDataType::OSP_GEOMETRIC_MODEL)
        .value("OSP_GEOMETRY", OSPDataType::OSP_GEOMETRY)
        .value("OSP_GROUP", OSPDataType::OSP_GROUP)
        .value("OSP_IMAGE_OPERATION", OSPDataType::OSP_IMAGE_OPERATION)
        .value("OSP_INSTANCE", OSPDataType::OSP_INSTANCE)
        .value("OSP_LIGHT", OSPDataType::OSP_LIGHT)
        .value("OSP_MATERIAL", OSPDataType::OSP_MATERIAL)
        .value("OSP_RENDERER", OSPDataType::OSP_RENDERER)
        .value("OSP_TEXTURE", OSPDataType::OSP_TEXTURE)
        .value("OSP_TRANSFER_FUNCTION", OSPDataType::OSP_TRANSFER_FUNCTION)
        .value("OSP_VOLUME", OSPDataType::OSP_VOLUME)
        .value("OSP_VOLUMETRIC_MODEL", OSPDataType::OSP_VOLUMETRIC_MODEL)
        .value("OSP_WORLD", OSPDataType::OSP_WORLD)
        .value("OSP_STRING", OSPDataType::OSP_STRING)
        .value("OSP_CHAR", OSPDataType::OSP_CHAR)
        .value("OSP_UCHAR", OSPDataType::OSP_UCHAR)
        .value("OSP_VEC2UC", OSPDataType::OSP_VEC2UC)
        .value("OSP_VEC3UC", OSPDataType::OSP_VEC3UC)
        .value("OSP_VEC4UC", OSPDataType::OSP_VEC4UC)
        .value("OSP_BYTE", OSPDataType::OSP_BYTE)
        .value("OSP_RAW", OSPDataType::OSP_RAW)
        .value("OSP_SHORT", OSPDataType::OSP_SHORT)
        .value("OSP_USHORT", OSPDataType::OSP_USHORT)
        .value("OSP_INT", OSPDataType::OSP_INT)
        .value("OSP_VEC2I", OSPDataType::OSP_VEC2I)
        .value("OSP_VEC3I", OSPDataType::OSP_VEC3I)
        .value("OSP_VEC4I", OSPDataType::OSP_VEC4I)
        .value("OSP_UINT", OSPDataType::OSP_UINT)
        .value("OSP_VEC2UI", OSPDataType::OSP_VEC2UI)
        .value("OSP_VEC3UI", OSPDataType::OSP_VEC3UI)
        .value("OSP_VEC4UI", OSPDataType::OSP_VEC4UI)
        .value("OSP_LONG", OSPDataType::OSP_LONG)
        .value("OSP_VEC2L", OSPDataType::OSP_VEC2L)
        .value("OSP_VEC3L", OSPDataType::OSP_VEC3L)
        .value("OSP_VEC4L", OSPDataType::OSP_VEC4L)
        .value("OSP_ULONG", OSPDataType::OSP_ULONG)
        .value("OSP_VEC2UL", OSPDataType::OSP_VEC2UL)
        .value("OSP_VEC3UL", OSPDataType::OSP_VEC3UL)
        .value("OSP_VEC4UL", OSPDataType::OSP_VEC4UL)
        .value("OSP_FLOAT", OSPDataType::OSP_FLOAT)
        .value("OSP_VEC2F", OSPDataType::OSP_VEC2F)
        .value("OSP_VEC3F", OSPDataType::OSP_VEC3F)
        .value("OSP_VEC4F", OSPDataType::OSP_VEC4F)
        .value("OSP_DOUBLE", OSPDataType::OSP_DOUBLE)
        .value("OSP_BOX1I", OSPDataType::OSP_BOX1I)
        .value("OSP_BOX2I", OSPDataType::OSP_BOX2I)
        .value("OSP_BOX3I", OSPDataType::OSP_BOX3I)
        .value("OSP_BOX4I", OSPDataType::OSP_BOX4I)
        .value("OSP_BOX1F", OSPDataType::OSP_BOX1F)
        .value("OSP_BOX2F", OSPDataType::OSP_BOX2F)
        .value("OSP_BOX3F", OSPDataType::OSP_BOX3F)
        .value("OSP_BOX4F", OSPDataType::OSP_BOX4F)
        .value("OSP_LINEAR2F", OSPDataType::OSP_LINEAR2F)
        .value("OSP_LINEAR3F", OSPDataType::OSP_LINEAR3F)
        .value("OSP_AFFINE2F", OSPDataType::OSP_AFFINE2F)
        .value("OSP_AFFINE3F", OSPDataType::OSP_AFFINE3F)
        .value("OSP_UNKNOWN", OSPDataType::OSP_UNKNOWN)
    ;
    
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
    
    // Device
    
    /*
    XXX Hmmm, incomplete type 'osp::Device' used in type trait expression
    m.def("get_current_device", []() {
            return ospray::cpp::Device(ospGetCurrentDevice());
        })
    ;
    
    py::class_<ospray::cpp::Device>(m, "Device")
        .def(py::init<const std::string &>(), py::arg("type")="default")
        .def("handle", &ospray::cpp::Device::handle)
    ;
    */    
    
    // Current device 
    m.def("set_error_func", [](py::function& func) { py_error_func = func; });
    m.def("set_status_func", [](py::function& func) { py_status_func = func; });
    
    // Scene
    
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
            [](py::tuple& imgsize, OSPFrameBufferFormat format, int channels) {
                return framebuffer_create(imgsize, format, channels);
            })//,
            //py::arg("format")=OSP_FB_SRGBA, py::arg("channels")=OSP_FB_COLOR)
            )
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