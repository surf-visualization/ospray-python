#include <pybind11/pybind11.h>
#include <pybind11/numpy.h>
#include <pybind11/stl.h>
#include <pybind11/operators.h>
#include <ospray/ospray_cpp.h>
#include <ospray/version.h>
#include <glm/mat4x4.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include "enums.h"
#include "conversion.h"
#include "mat.h"
//#include "testing.h"

namespace py = pybind11;

typedef ospray::cpp::ManagedObject<OSPCamera, OSP_CAMERA>                   ManagedCamera;
typedef ospray::cpp::ManagedObject<OSPData, OSP_DATA>                       ManagedData;
typedef ospray::cpp::ManagedObject<OSPFrameBuffer, OSP_FRAMEBUFFER>         ManagedFrameBuffer;
typedef ospray::cpp::ManagedObject<OSPFuture, OSP_FUTURE>                   ManagedFuture;
typedef ospray::cpp::ManagedObject<OSPGeometricModel, OSP_GEOMETRIC_MODEL>  ManagedGeometricModel;
typedef ospray::cpp::ManagedObject<OSPGeometry, OSP_GEOMETRY>               ManagedGeometry;
typedef ospray::cpp::ManagedObject<OSPGroup, OSP_GROUP>                     ManagedGroup;
typedef ospray::cpp::ManagedObject<OSPImageOperation, OSP_IMAGE_OPERATION>  ManagedImageOperation;
typedef ospray::cpp::ManagedObject<OSPInstance, OSP_INSTANCE>               ManagedInstance;
typedef ospray::cpp::ManagedObject<OSPLight, OSP_LIGHT>                     ManagedLight;
typedef ospray::cpp::ManagedObject<OSPMaterial, OSP_MATERIAL>               ManagedMaterial;
typedef ospray::cpp::ManagedObject<OSPRenderer, OSP_RENDERER>               ManagedRenderer;
typedef ospray::cpp::ManagedObject<OSPTexture, OSP_TEXTURE>                 ManagedTexture;
typedef ospray::cpp::ManagedObject<OSPTransferFunction, OSP_TRANSFER_FUNCTION> ManagedTransferFunction;
typedef ospray::cpp::ManagedObject<OSPVolume, OSP_VOLUME>                   ManagedVolume;
typedef ospray::cpp::ManagedObject<OSPVolumetricModel, OSP_VOLUMETRIC_MODEL> ManagedVolumetricModel;
typedef ospray::cpp::ManagedObject<OSPWorld, OSP_WORLD>                     ManagedWorld;

static py::function py_error_callback;
static py::function py_status_callback;

// As we unconditionally override the OSPRay default status and error handlers
// in init() we need to handle the situation where no Python-level handlers are set.
// Produce output on stdout directly in those cases, as otherwise there will be
// no output, nor any way to get output.

static void
error_func(void* /*userdata*/, OSPError error, const char *details)
{
    if (py_error_callback)
        py_error_callback(error, details);
    else
        printf("OSPRAY ERROR: %d (%s)\n", error, details);
}

static void
status_func(void* /*userdata*/, const char *message)
{
    if (py_status_callback)
        py_status_callback(message);
    else
        printf("OSPRAY STATUS: %s\n", message);    
}

static void
throw_osperror(const std::string& prefix, OSPError e)
{
    std::string msg = prefix + ": ";
    std::string error;
    
    switch (e)
    {
      case OSP_NO_ERROR          : error="No error has been recorded"; break;
      case OSP_UNKNOWN_ERROR     : error="An unknown error has occurred"; break;
      case OSP_INVALID_ARGUMENT  : error="An invalid argument is specified"; break;
      case OSP_INVALID_OPERATION : error="The operation is not allowed for the specified object"; break;
      case OSP_OUT_OF_MEMORY     : error="There is not enough memory left to execute the command"; break;
      case OSP_UNSUPPORTED_CPU   : error="The CPU is not supported as it does not support SSE4.1"; break;
      case OSP_VERSION_MISMATCH  : error="A module could not be loaded due to mismatching version"; break;
    }

    msg = msg + error;
    
    throw std::runtime_error(msg);
}
    
static py::tuple 
runtime_version()
{
    ospray::cpp::Device device(ospGetCurrentDevice());
    
    return py::make_tuple(
        ospDeviceGetProperty(device.handle(), OSP_DEVICE_VERSION_MAJOR),
        ospDeviceGetProperty(device.handle(), OSP_DEVICE_VERSION_MINOR),
        ospDeviceGetProperty(device.handle(), OSP_DEVICE_VERSION_PATCH)
    );
}

static std::vector<std::string>
//init(const py::module& m, const std::vector<std::string>& args)
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
        throw_osperror("ospInit() failed", res);
    }
    
    std::vector<std::string> newargs;
    
    for (int i = 0; i < argc; i++)
        newargs.push_back(std::string(argv[i]));
    
    delete [] argv;
    
    // Set C++ wrappers for error and status callbacks
    
    ospray::cpp::Device device(ospGetCurrentDevice());
    ospDeviceSetErrorCallback(device.handle(), error_func, nullptr);
    ospDeviceSetStatusCallback(device.handle(), status_func, nullptr);
    
    /*
    py::tuple rt_version = runtime_version();
    if (rt_version() != version_compiled)
        py::print("WARNING: run-time version", version_runtime, "does not match compile-time version", version_compiled);
    */
    
    return newargs;
}

static void
load_module(const std::string& name)
{
    OSPError res = ospLoadModule(name.c_str());
    
    if (res != OSP_NO_ERROR)
        throw_osperror("ospLoadModule('" + name + "') failed", res);
}


static void
print_array_info(const py::array &array)
{
    const py::dtype& dtype = array.dtype();
    
    printf("dimension %ld, shape (", array.ndim());
    for (int i = 0; i < array.ndim(); i++)
    {
        if (i > 0) printf("x");
        printf("%ld", array.shape(i));
    }
    printf("), dtype kind '%c' itemsize %ld\n", dtype.kind(), dtype.itemsize());
}

ospray::cpp::CopiedData
copied_data_from_numpy_array(const py::array& array)
{
    const int ndim = array.ndim();
    
    if (ndim > 3)
    {
        printf("ERROR: more than 3 dimensions not supported in copied_data_from_numpy_array(): ");
        print_array_info(array);
        printf("\n");
        return ospray::cpp::CopiedData();
    }
    
    const py::dtype& dtype = array.dtype();
    vec3ul num_items { 1, 1, 1 };
    vec3ul byte_stride { 0, 0, 0 };

    num_items.x = array.shape(0);
    if (ndim >= 2)
        num_items.y = array.shape(1);
    if (ndim >= 3)
        num_items.z = array.shape(2);
        
    // https://github.com/pybind/pybind11/issues/563#issuecomment-267836074
    // Use 
    //  py::isinstance<py::array_t<std::int32_t>>(buf) 
    // instead of 
    //  buf.dtype() == pybind11::dtype::of<std::int32_t>()

    if (py::isinstance<py::array_t<float>>(array))
        return ospray::cpp::CopiedData(array.data(), OSP_FLOAT, num_items, byte_stride);
    /*else if (py::isinstance<py::array_t<double>>(array))
        return ospray::cpp::CopiedData(array.data(), OSP_DOUBLE, num_items, byte_stride);*/
    /*else if (py::isinstance<py::array_t<int8_t>>(array))
        return ospray::cpp::CopiedData(array.data(), OSP_CHAR, num_items, byte_stride);*/
    else if (py::isinstance<py::array_t<uint8_t>>(array))
        return ospray::cpp::CopiedData(array.data(), OSP_UCHAR, num_items, byte_stride);
    /*else if (py::isinstance<py::array_t<int16_t>>(array))
        return ospray::cpp::CopiedData(array.data(), ???, num_items, byte_stride);
    else if (py::isinstance<py::array_t<uint16_t>>(array))
        return ospray::cpp::CopiedData(array.data(), ???, num_items, byte_stride);*/
    else if (py::isinstance<py::array_t<int32_t>>(array))
        return ospray::cpp::CopiedData(array.data(), OSP_INT, num_items, byte_stride);
    else if (py::isinstance<py::array_t<uint32_t>>(array))
        return ospray::cpp::CopiedData(array.data(), OSP_UINT, num_items, byte_stride);
    else if (py::isinstance<py::array_t<int64_t>>(array))
        return ospray::cpp::CopiedData(array.data(), OSP_LONG, num_items, byte_stride);
    else if (py::isinstance<py::array_t<uint64_t>>(array))
        return ospray::cpp::CopiedData(array.data(), OSP_ULONG, num_items, byte_stride);
        
    printf("WARNING: unhandled array in copied_data_from_numpy_array(): ");
    print_array_info(array);
    printf("\n");
    
    return ospray::cpp::CopiedData();
}

ospray::cpp::SharedData
shared_data_from_numpy_array(const py::array& array)
{
    const int ndim = array.ndim();
    
    if (ndim > 3)
    {
        printf("ERROR: more than 3 dimensions not supported in shared_data_from_numpy_array(): ");
        print_array_info(array);
        printf("\n");
        return ospray::cpp::SharedData();
    }
    
    const py::dtype& dtype = array.dtype();
    vec3ul num_items { 1, 1, 1 };
    vec3ul byte_stride { 0, 0, 0 };

    num_items.x = array.shape(0);
    if (ndim >= 2)
        num_items.y = array.shape(1);
    if (ndim >= 3)
        num_items.z = array.shape(2);
        
    // https://github.com/pybind/pybind11/issues/563#issuecomment-267836074
    // Use 
    //  py::isinstance<py::array_t<std::int32_t>>(buf) 
    // instead of 
    //  buf.dtype() == pybind11::dtype::of<std::int32_t>()

    if (py::isinstance<py::array_t<float>>(array))
        return ospray::cpp::SharedData(array.data(), OSP_FLOAT, num_items, byte_stride);
    /*else if (py::isinstance<py::array_t<double>>(array))
        return ospray::cpp::SharedData(array.data(), OSP_DOUBLE, num_items, byte_stride);*/
    /*else if (py::isinstance<py::array_t<int8_t>>(array))
        return ospray::cpp::SharedData(array.data(), OSP_CHAR, num_items, byte_stride);*/
    else if (py::isinstance<py::array_t<uint8_t>>(array))
        return ospray::cpp::SharedData(array.data(), OSP_UCHAR, num_items, byte_stride);
    /*else if (py::isinstance<py::array_t<int16_t>>(array))
        return ospray::cpp::SharedData(array.data(), ???, num_items, byte_stride);
    else if (py::isinstance<py::array_t<uint16_t>>(array))
        return ospray::cpp::SharedData(array.data(), ???, num_items, byte_stride);*/
    else if (py::isinstance<py::array_t<int32_t>>(array))
        return ospray::cpp::SharedData(array.data(), OSP_INT, num_items, byte_stride);
    else if (py::isinstance<py::array_t<uint32_t>>(array))
        return ospray::cpp::SharedData(array.data(), OSP_UINT, num_items, byte_stride);
    else if (py::isinstance<py::array_t<int64_t>>(array))
        return ospray::cpp::SharedData(array.data(), OSP_LONG, num_items, byte_stride);
    else if (py::isinstance<py::array_t<uint64_t>>(array))
        return ospray::cpp::SharedData(array.data(), OSP_ULONG, num_items, byte_stride);
        
    printf("WARNING: unhandled array in shared_data_from_numpy_array(): ");
    print_array_info(array);
    printf("\n");
    
    return ospray::cpp::SharedData();
}

// Turn a numpy array of shape (..., 2|3|4) into a Data object of
// the corresponding vec<n><t> type
/*
    typedef vec_t<uint8_t, 4> vec4uc;
    typedef vec_t<int8_t, 4> vec4c;
    typedef vec_t<uint32_t, 4> vec4ui;
    typedef vec_t<int32_t, 4> vec4i;
    typedef vec_t<uint64_t, 4> vec4ul;
    typedef vec_t<int64_t, 4> vec4l;
    typedef vec_t<float, 4> vec4f;
    typedef vec_t<double, 4> vec4d;
*/
ospray::cpp::CopiedData
copied_data_from_numpy_array_vec(const py::array& array)
{
    const int ndim = array.ndim();
    
    if (ndim > 3)
    {
        printf("ERROR: more than 3 dimensions not supported in copied_data_from_numpy_array_vec(): ");
        print_array_info(array);
        printf("\n");
        return ospray::cpp::CopiedData();
    }
    
    const int vecdim = array.shape(ndim-1);
    
    if (vecdim < 2 || vecdim > 4)
    {
        printf("ERROR: last dimension needs to be in range 2-4 in copied_data_from_numpy_array_vec(): ");
        print_array_info(array);
        printf("\n");
        return ospray::cpp::CopiedData();
    }

    const py::dtype& dtype = array.dtype();
    vec3ul num_items { 1, 1, 1 };
    vec3ul byte_stride { 0, 0, 0 };

    num_items.x = array.shape(0);
    if (ndim > 2)
        num_items.y = array.shape(1);
    if (ndim > 3)
        num_items.z = array.shape(2);

    if (vecdim == 2)
    {
        if (py::isinstance<py::array_t<float>>(array))
            return ospray::cpp::CopiedData(array.data(), OSP_VEC2F, num_items, byte_stride);
        else if (py::isinstance<py::array_t<double>>(array))
            return ospray::cpp::CopiedData(array.data(), OSP_VEC2D, num_items, byte_stride);
        else if (py::isinstance<py::array_t<int8_t>>(array))
            return ospray::cpp::CopiedData(array.data(), OSP_VEC2C, num_items, byte_stride);
        else if (py::isinstance<py::array_t<uint8_t>>(array))
            return ospray::cpp::CopiedData(array.data(), OSP_VEC2UC, num_items, byte_stride);
        else if (py::isinstance<py::array_t<int32_t>>(array))
            return ospray::cpp::CopiedData(array.data(), OSP_VEC2I, num_items, byte_stride);
        else if (py::isinstance<py::array_t<uint32_t>>(array))
            return ospray::cpp::CopiedData(array.data(), OSP_VEC2UI, num_items, byte_stride);
        else if (py::isinstance<py::array_t<int64_t>>(array))
            return ospray::cpp::CopiedData(array.data(), OSP_VEC2L, num_items, byte_stride);
        else if (py::isinstance<py::array_t<uint64_t>>(array))
            return ospray::cpp::CopiedData(array.data(), OSP_VEC2UL, num_items, byte_stride);
    }       
    else if (vecdim == 3)
    {
        if (py::isinstance<py::array_t<float>>(array))
            return ospray::cpp::CopiedData(array.data(), OSP_VEC3F, num_items, byte_stride);
        else if (py::isinstance<py::array_t<double>>(array))
            return ospray::cpp::CopiedData(array.data(), OSP_VEC3D, num_items, byte_stride);
        else if (py::isinstance<py::array_t<int8_t>>(array))
            return ospray::cpp::CopiedData(array.data(), OSP_VEC3C, num_items, byte_stride);
        else if (py::isinstance<py::array_t<uint8_t>>(array))
            return ospray::cpp::CopiedData(array.data(), OSP_VEC3UC, num_items, byte_stride);
        else if (py::isinstance<py::array_t<int32_t>>(array))
            return ospray::cpp::CopiedData(array.data(), OSP_VEC3I, num_items, byte_stride);
        else if (py::isinstance<py::array_t<uint32_t>>(array))
            return ospray::cpp::CopiedData(array.data(), OSP_VEC3UI, num_items, byte_stride);
        else if (py::isinstance<py::array_t<int64_t>>(array))
            return ospray::cpp::CopiedData(array.data(), OSP_VEC3L, num_items, byte_stride);
        else if (py::isinstance<py::array_t<uint64_t>>(array))
            return ospray::cpp::CopiedData(array.data(), OSP_VEC3UL, num_items, byte_stride);
    }
    else
    {
        if (py::isinstance<py::array_t<float>>(array))
            return ospray::cpp::CopiedData(array.data(), OSP_VEC4F, num_items, byte_stride);
        else if (py::isinstance<py::array_t<double>>(array))
            return ospray::cpp::CopiedData(array.data(), OSP_VEC4D, num_items, byte_stride);
        else if (py::isinstance<py::array_t<int8_t>>(array))
            return ospray::cpp::CopiedData(array.data(), OSP_VEC4C, num_items, byte_stride);
        else if (py::isinstance<py::array_t<uint8_t>>(array))
            return ospray::cpp::CopiedData(array.data(), OSP_VEC4UC, num_items, byte_stride);
        else if (py::isinstance<py::array_t<int32_t>>(array))
            return ospray::cpp::CopiedData(array.data(), OSP_VEC4I, num_items, byte_stride);
        else if (py::isinstance<py::array_t<uint32_t>>(array))
            return ospray::cpp::CopiedData(array.data(), OSP_VEC4UI, num_items, byte_stride);
        else if (py::isinstance<py::array_t<int64_t>>(array))
            return ospray::cpp::CopiedData(array.data(), OSP_VEC4L, num_items, byte_stride);
        else if (py::isinstance<py::array_t<uint64_t>>(array))
            return ospray::cpp::CopiedData(array.data(), OSP_VEC4UL, num_items, byte_stride);
    }
    
    printf("WARNING: unhandled array in copied_data_from_numpy_array_vec(): ");
    print_array_info(array);
    printf("\n");
        
    return ospray::cpp::CopiedData();
}

ospray::cpp::SharedData
shared_data_from_numpy_array_vec(const py::array& array)
{
    const int ndim = array.ndim();
    
    if (ndim > 3)
    {
        printf("ERROR: more than 3 dimensions not supported in shared_data_from_numpy_array_vec(): ");
        print_array_info(array);
        printf("\n");
        return ospray::cpp::SharedData();
    }
    
    const int vecdim = array.shape(ndim-1);
    
    if (vecdim < 2 || vecdim > 4)
    {
        printf("ERROR: last dimension needs to be in range 2-4 in shared_data_from_numpy_array_vec(): ");
        print_array_info(array);
        printf("\n");
        return ospray::cpp::SharedData();
    }
    
    const py::dtype& dtype = array.dtype();
    vec3ul num_items { 1, 1, 1 };
    vec3ul byte_stride { 0, 0, 0 };

    num_items.x = array.shape(0);
    if (ndim > 2)
        num_items.y = array.shape(1);
    if (ndim > 3)
        num_items.z = array.shape(2);

    if (vecdim == 2)
    {
        if (py::isinstance<py::array_t<float>>(array))
            return ospray::cpp::SharedData(array.data(), OSP_VEC2F, num_items, byte_stride);
        else if (py::isinstance<py::array_t<double>>(array))
            return ospray::cpp::SharedData(array.data(), OSP_VEC2D, num_items, byte_stride);
        else if (py::isinstance<py::array_t<int8_t>>(array))
            return ospray::cpp::SharedData(array.data(), OSP_VEC2C, num_items, byte_stride);
        else if (py::isinstance<py::array_t<uint8_t>>(array))
            return ospray::cpp::SharedData(array.data(), OSP_VEC2UC, num_items, byte_stride);
        else if (py::isinstance<py::array_t<int32_t>>(array))
            return ospray::cpp::SharedData(array.data(), OSP_VEC2I, num_items, byte_stride);
        else if (py::isinstance<py::array_t<uint32_t>>(array))
            return ospray::cpp::SharedData(array.data(), OSP_VEC2UI, num_items, byte_stride);
        else if (py::isinstance<py::array_t<int64_t>>(array))
            return ospray::cpp::SharedData(array.data(), OSP_VEC2L, num_items, byte_stride);
        else if (py::isinstance<py::array_t<uint64_t>>(array))
            return ospray::cpp::SharedData(array.data(), OSP_VEC2UL, num_items, byte_stride);
    }       
    else if (vecdim == 3)
    {
        if (py::isinstance<py::array_t<float>>(array))
            return ospray::cpp::SharedData(array.data(), OSP_VEC3F, num_items, byte_stride);
        else if (py::isinstance<py::array_t<double>>(array))
            return ospray::cpp::SharedData(array.data(), OSP_VEC3D, num_items, byte_stride);
        else if (py::isinstance<py::array_t<int8_t>>(array))
            return ospray::cpp::SharedData(array.data(), OSP_VEC3C, num_items, byte_stride);
        else if (py::isinstance<py::array_t<uint8_t>>(array))
            return ospray::cpp::SharedData(array.data(), OSP_VEC3UC, num_items, byte_stride);
        else if (py::isinstance<py::array_t<int32_t>>(array))
            return ospray::cpp::SharedData(array.data(), OSP_VEC3I, num_items, byte_stride);
        else if (py::isinstance<py::array_t<uint32_t>>(array))
            return ospray::cpp::SharedData(array.data(), OSP_VEC3UI, num_items, byte_stride);
        else if (py::isinstance<py::array_t<int64_t>>(array))
            return ospray::cpp::SharedData(array.data(), OSP_VEC3L, num_items, byte_stride);
        else if (py::isinstance<py::array_t<uint64_t>>(array))
            return ospray::cpp::SharedData(array.data(), OSP_VEC3UL, num_items, byte_stride);
    }
    else
    {
        if (py::isinstance<py::array_t<float>>(array))
            return ospray::cpp::SharedData(array.data(), OSP_VEC4F, num_items, byte_stride);
        else if (py::isinstance<py::array_t<double>>(array))
            return ospray::cpp::SharedData(array.data(), OSP_VEC4D, num_items, byte_stride);
        else if (py::isinstance<py::array_t<int8_t>>(array))
            return ospray::cpp::SharedData(array.data(), OSP_VEC4C, num_items, byte_stride);
        else if (py::isinstance<py::array_t<uint8_t>>(array))
            return ospray::cpp::SharedData(array.data(), OSP_VEC4UC, num_items, byte_stride);
        else if (py::isinstance<py::array_t<int32_t>>(array))
            return ospray::cpp::SharedData(array.data(), OSP_VEC4I, num_items, byte_stride);
        else if (py::isinstance<py::array_t<uint32_t>>(array))
            return ospray::cpp::SharedData(array.data(), OSP_VEC4UI, num_items, byte_stride);
        else if (py::isinstance<py::array_t<int64_t>>(array))
            return ospray::cpp::SharedData(array.data(), OSP_VEC4L, num_items, byte_stride);
        else if (py::isinstance<py::array_t<uint64_t>>(array))
            return ospray::cpp::SharedData(array.data(), OSP_VEC4UL, num_items, byte_stride);
    }
    
    printf("WARNING: unhandled array in shared_data_from_numpy_array_vec(): ");
    print_array_info(array);
    printf("\n");
        
    return ospray::cpp::SharedData();
}



// Turn a numpy array of shape (..., 2|4|6|8) into a Data object of
// the corresponding box<n><t> type
/*
    using box1i  = range_t<int32_t>;
    using box2i  = box_t<int32_t, 2>;
    using box3i  = box_t<int32_t, 3>;
    using box4i  = box_t<int32_t, 4>;
    using box1f  = range_t<float>;
    using box2f  = box_t<float, 2>;
    using box3f  = box_t<float, 3>;
    using box4f  = box_t<float, 4>;
    //using box3fa = box_t<float, 3, 1>;
*/

ospray::cpp::CopiedData
copied_data_from_numpy_array_box(const py::array& array)
{
    const int ndim = array.ndim();
    
    if (ndim > 3)
    {
        printf("ERROR: more than 3 dimensions not supported in copied_data_from_numpy_array_box(): ");
        print_array_info(array);
        printf("\n");
        return ospray::cpp::CopiedData();
    }
    
    const int vecdim = array.shape(ndim-1);
    
    if (vecdim != 2 && vecdim != 4 && vecdim != 6 && vecdim != 8)
    {
        printf("ERROR: last dimension needs to be 2,4,6 or 8 in copied_data_from_numpy_array_box(): ");
        print_array_info(array);
        printf("\n");
        return ospray::cpp::CopiedData();
    }
    
    const py::dtype& dtype = array.dtype();
    vec3ul  num_items { 1, 1, 1 };
    vec3ul  byte_stride { 0, 0, 0 };
    
    num_items.x = array.shape(0);
    if (ndim > 2)
        num_items.y = array.shape(1);
    if (ndim > 3)
        num_items.z = array.shape(2);
    
    if (vecdim == 2)
    {
        if (py::isinstance<py::array_t<float>>(array))
            return ospray::cpp::CopiedData(array.data(), OSP_BOX1F, num_items, byte_stride);
        else if (py::isinstance<py::array_t<int32_t>>(array))
            return ospray::cpp::CopiedData(array.data(), OSP_BOX1I, num_items, byte_stride);
    }       
    else if (vecdim == 4)
    {
        if (py::isinstance<py::array_t<float>>(array))
            return ospray::cpp::CopiedData(array.data(), OSP_BOX2F, num_items, byte_stride);
        else if (py::isinstance<py::array_t<int32_t>>(array))
            return ospray::cpp::CopiedData(array.data(), OSP_BOX2I, num_items, byte_stride);
    }
    else if (vecdim == 6)
    {
        if (py::isinstance<py::array_t<float>>(array))
            return ospray::cpp::CopiedData(array.data(), OSP_BOX3F, num_items, byte_stride);
        else if (py::isinstance<py::array_t<int32_t>>(array))
            return ospray::cpp::CopiedData(array.data(), OSP_BOX3I, num_items, byte_stride);
    }
    else
    {
        if (py::isinstance<py::array_t<float>>(array))
            return ospray::cpp::CopiedData(array.data(), OSP_BOX4F, num_items, byte_stride);
        else if (py::isinstance<py::array_t<int32_t>>(array))
            return ospray::cpp::CopiedData(array.data(), OSP_BOX4I, num_items, byte_stride);
    }
    
    printf("WARNING: unhandled array in copied_data_from_numpy_array_box(): ");
    print_array_info(array);
    printf("\n");
        
    return ospray::cpp::CopiedData();
}

ospray::cpp::SharedData
shared_data_from_numpy_array_box(const py::array& array)
{
    const int ndim = array.ndim();
    
    if (ndim > 3)
    {
        printf("ERROR: more than 3 dimensions not supported in shared_data_from_numpy_array_box(): ");
        print_array_info(array);
        printf("\n");
        return ospray::cpp::SharedData();
    }
    
    const int vecdim = array.shape(ndim-1);
    
    if (vecdim != 2 && vecdim != 4 && vecdim != 6 && vecdim != 8)
    {
        printf("ERROR: last dimension needs to be 2,4,6 or 8 in shared_data_from_numpy_array_box(): ");
        print_array_info(array);
        printf("\n");
        return ospray::cpp::SharedData();
    }
    
    const py::dtype& dtype = array.dtype();
    vec3ul  num_items { 1, 1, 1 };
    vec3ul  byte_stride { 0, 0, 0 };
    
    num_items.x = array.shape(0);
    if (ndim > 2)
        num_items.y = array.shape(1);
    if (ndim > 3)
        num_items.z = array.shape(2);
    
    if (vecdim == 2)
    {
        if (py::isinstance<py::array_t<float>>(array))
            return ospray::cpp::SharedData(array.data(), OSP_BOX1F, num_items, byte_stride);
        else if (py::isinstance<py::array_t<int32_t>>(array))
            return ospray::cpp::SharedData(array.data(), OSP_BOX1I, num_items, byte_stride);
    }       
    else if (vecdim == 4)
    {
        if (py::isinstance<py::array_t<float>>(array))
            return ospray::cpp::SharedData(array.data(), OSP_BOX2F, num_items, byte_stride);
        else if (py::isinstance<py::array_t<int32_t>>(array))
            return ospray::cpp::SharedData(array.data(), OSP_BOX2I, num_items, byte_stride);
    }
    else if (vecdim == 6)
    {
        if (py::isinstance<py::array_t<float>>(array))
            return ospray::cpp::SharedData(array.data(), OSP_BOX3F, num_items, byte_stride);
        else if (py::isinstance<py::array_t<int32_t>>(array))
            return ospray::cpp::SharedData(array.data(), OSP_BOX3I, num_items, byte_stride);
    }
    else
    {
        if (py::isinstance<py::array_t<float>>(array))
            return ospray::cpp::SharedData(array.data(), OSP_BOX4F, num_items, byte_stride);
        else if (py::isinstance<py::array_t<int32_t>>(array))
            return ospray::cpp::SharedData(array.data(), OSP_BOX4I, num_items, byte_stride);
    }
    
    printf("WARNING: unhandled array in shared_data_from_numpy_array_box(): ");
    print_array_info(array);
    printf("\n");
        
    return ospray::cpp::SharedData();
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
set_param_string(T &self, const std::string &name, const std::string &value)
{
    self.setParam(name, value);
}

template<typename T>
void
set_param_copied_data(T &self, const std::string &name, const ospray::cpp::CopiedData &data)
{
    self.setParam(name, data);
}

template<typename T>
void
set_param_shared_data(T &self, const std::string &name, const ospray::cpp::SharedData &data)
{
    self.setParam(name, data);
}

static std::string
determine_tuple_type(const py::tuple &value)
{
    std::string res = "int";
    
    for (size_t i = 0; i < value.size(); i++)
    {
        if (py::isinstance<py::float_>(value[i]))
            res = "float";
        else if (!py::isinstance<py::int_>(value[i]))
            return value[i].get_type().attr("__name__").cast<std::string>();
    }
    
    return res;
}

template<typename T>
void
set_param_tuple(T &self, const std::string &name, const py::tuple &value)
{
    size_t n = value.size();    
    
    if (n < 2 || n > 4)
    {
        printf("ERROR: in set_param_tuple(..., '%s', ...), tuple length should be in range 2-4!\n", name.c_str());
        return;
    }
    
    std::string tuple_type = determine_tuple_type(value);
    
    if (tuple_type != "int" && tuple_type != "float")
    {
        printf("ERROR: unhandled data type (%s) in set_param_tuple(..., '%s', ...)!\n", tuple_type.c_str(), name.c_str());
        return;
    }
    
    bool ints = tuple_type == "int";

    if (n == 2)
    {
        if (ints)
        {
            vec2i vvalue;
            
            vvalue.x = py::cast<int>(value[0]);
            vvalue.y = py::cast<int>(value[1]);
            
            self.setParam(name, vvalue);
        }
        else
        {
            vec2f vvalue;
            
            vvalue.x = py::cast<float>(value[0]);
            vvalue.y = py::cast<float>(value[1]);
            
            self.setParam(name, vvalue);
        }
    }
    
    else if (n == 3)
    {
        if (ints)
        {
            vec3i vvalue;
            
            vvalue.x = py::cast<int>(value[0]);
            vvalue.y = py::cast<int>(value[1]);
            vvalue.z = py::cast<int>(value[2]);
            
            self.setParam(name, vvalue);
        }
        else 
        {
            vec3f vvalue;
            
            vvalue.x = py::cast<float>(value[0]);
            vvalue.y = py::cast<float>(value[1]);
            vvalue.z = py::cast<float>(value[2]);
            
            self.setParam(name, vvalue);
        }
    }
    
    else if (n == 4)
    {
        if (ints)
        {
            vec4i vvalue;
            
            vvalue.x = py::cast<int>(value[0]);
            vvalue.y = py::cast<int>(value[1]);
            vvalue.z = py::cast<int>(value[2]);
            vvalue.w = py::cast<int>(value[3]);
            
            self.setParam(name, vvalue);
        }
        else
        {
            vec4f vvalue;
            
            vvalue.x = py::cast<float>(value[0]);
            vvalue.y = py::cast<float>(value[1]);
            vvalue.z = py::cast<float>(value[2]);
            vvalue.w = py::cast<float>(value[3]);
            
            self.setParam(name, vvalue);
        }
    }
}

template<typename T>
ospray::cpp::CopiedData
build_data_from_list(const std::string &listcls, const py::list &values)
{
    std::vector<T> items;
        
    for (size_t i = 0; i < values.size(); i++)
    {
        auto item = values[i];
        std::string itemcls = item.get_type().attr("__name__").cast<std::string>();
        
        if (itemcls != listcls)
        {
            printf("ERROR: item %lu in list is not of type '%s', but of type '%s'!\n", 
                i, listcls.c_str(), itemcls.c_str());
            
            return ospray::cpp::CopiedData();
        }
        
        items.push_back(values[i].cast<T>());
    }
    
    return ospray::cpp::CopiedData(items);
}

// List assumed to only contain OSPObject's of the same type
template<typename T>
void
set_param_list(T &self, const std::string &name, const py::list &values)
{
    if (values.size() == 0)
    {
        printf("WARNING: not setting empty list in set_param_list(..., '%s', ...)\n", name.c_str());
        return;
    }
    
    auto first = values[0];
    
    std::string listcls = first.get_type().attr("__name__").cast<std::string>();
    
    if (listcls == "GeometricModel")
        self.setParam(name, build_data_from_list<ospray::cpp::GeometricModel>(listcls, values));
    else if (listcls == "ImageOperation")
        self.setParam(name, build_data_from_list<ospray::cpp::ImageOperation>(listcls, values));
    else if (listcls == "Instance")
        self.setParam(name, build_data_from_list<ospray::cpp::Instance>(listcls, values));
    else if (listcls == "Light")
        self.setParam(name, build_data_from_list<ospray::cpp::Light>(listcls, values));
    else if (listcls == "Material")
        self.setParam(name, build_data_from_list<ospray::cpp::Material>(listcls, values));
    else if (listcls == "VolumetricModel")
        self.setParam(name, build_data_from_list<ospray::cpp::VolumetricModel>(listcls, values));
    else
        printf("WARNING: unhandled list with items of type %s in set_param_list()!\n", listcls.c_str());
}

template<typename T>
void
set_param_numpy_array(T &self, const std::string &name, py::array &array)
{
    self.setParam(name, copied_data_from_numpy_array(array));
}

template<typename T>
void
set_param_mat4(T &self, const std::string &name, const glm::mat4 &value)
{
    float xform[12];
    affine3fv_from_mat4(xform, value);
    /*
    printf("mat4: ");
    for (int r = 0; r < 4; r++)
    {
        for (int c = 0; c < 3; c++)
            printf("%.6f ", xform[r*3+c]);
        printf("\n");
    }
    */
    self.setParam(name, OSP_AFFINE3F, xform);
}

template<typename T>
void
set_param_material(T &self, const std::string &name, const ospray::cpp::Material &value)
{
    self.setParam(name, value);
}

template<typename T>
void
set_param_texture(T &self, const std::string &name, const ospray::cpp::Texture &value)
{
    self.setParam(name, value);
}

template<typename T>
void
set_param_transfer_function(T &self, const std::string &name, const ospray::cpp::TransferFunction &value)
{
    self.setParam(name, value);
}

template<typename T>
void
set_param_volume(T &self, const std::string &name, const ospray::cpp::Volume &value)
{
    self.setParam(name, value);
}

template<typename T>
void
set_param_volumetric_model(T &self, const std::string &name, const ospray::cpp::VolumetricModel &value)
{
    self.setParam(name, value);
}

template<typename T>
void
remove_param(T &self, const std::string &name)
{
    self.removeParam(name.c_str());
}

template<typename T>
py::tuple
get_bounds(T &self)
{
    const box3f bounds = self.template getBounds<box3f>();  // XXX ugliest C++ syntax ever...
    
    return py::make_tuple(
        bounds.lower.x, bounds.lower.y, bounds.lower.z,
        bounds.upper.x, bounds.upper.y, bounds.upper.z
    );
}

/*
template<typename T>
py::tuple
get_handle(T &self)
{
    return self.handle();
}
*/

template<typename T>
bool
same_handle(T &self, const T &other)
{
    return self.handle() == other.handle();
}

template<typename T>
void
declare_managedobject(py::module &m, const char *name)
{
    py::class_<T>(m, name)
        //(void (T::*)(const std::string &, const float &)) 
        // XXX can probably replace most of these with lambas here
        .def("set_param", &set_param_bool<T>)
        .def("set_param", &set_param_int<T>)
        .def("set_param", &set_param_float<T>)
        .def("set_param", &set_param_string<T>)
        .def("set_param", &set_param_tuple<T>)
        .def("set_param", &set_param_list<T>)
        .def("set_param", &set_param_copied_data<T>)
        .def("set_param", &set_param_shared_data<T>)
        .def("set_param", &set_param_numpy_array<T>)
        .def("set_param", &set_param_mat4<T>)
        .def("set_param", &set_param_material<T>)
        .def("set_param", &set_param_texture<T>)
        .def("set_param", &set_param_transfer_function<T>)
        .def("set_param", &set_param_volume<T>)
        .def("set_param", &set_param_volumetric_model<T>)
        .def("remove_param", &remove_param<T>) 
        .def("commit", &T::commit)
        .def("get_bounds", &get_bounds<T>)
        //.def("handle", &get_handle<T>)      // XXX no viable conversion 
        .def("same_handle", &same_handle<T>)
        .def("__repr__", 
            [name](const T& self) {
                std::stringstream stream;
                stream << std::hex << (size_t)(self.handle());
                return "<ospray." + std::string(name).substr(7) + " referencing 0x" + stream.str() + ">";
            })
        .def("__enter__", [](const T& /*self*/) { /* no-op */ })
        .def("__exit__", [](const T& self, py::object /*exc_type*/, py::object /*exc_value*/, py::object /*traceback*/) {
                self.commit();
            })
    ;
}


// FrameBuffer

py::array
framebuffer_get(ospray::cpp::FrameBuffer &self, OSPFrameBufferChannel channel, py::tuple &imgsize, OSPFrameBufferFormat format=OSP_FB_NONE)
{
    if (channel == OSP_FB_ACCUM || channel == OSP_FB_VARIANCE)
        throw std::invalid_argument("requested framebuffer channel cannot be mapped");
    
    int w = py::cast<int>(imgsize[0]);
    int h = py::cast<int>(imgsize[1]);
    py::array res;
    void *fb;
    
    fb = self.map(channel);
    
    if (fb == nullptr)
        throw std::invalid_argument("requested framebuffer channel is not available");
    
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
    else if (channel == OSP_FB_NORMAL)
    {
        res = py::array_t<float>({w,h,3}, (float*)fb);
    }
    else if (channel == OSP_FB_ALBEDO)
    {
        res = py::array_t<float>({w,h,3}, (float*)fb);
    }
    
    self.unmap(fb);
    
    return res;
}

// glm::mat4

glm::mat4
make_identity()
{
    return glm::mat4(1.0f);
}

glm::mat4
make_rotate(float degrees, float x, float y, float z)
{
    return glm::rotate(glm::radians(degrees), glm::vec3(x, y, z));
}

glm::mat4
make_from_quaternion(float w, float x, float y, float z)
{
    return glm::mat4_cast(glm::quat(w, x, y, z));
}

glm::mat4
make_translate(float x, float y, float z)
{
    return glm::translate(glm::vec3(x, y, z));
}

glm::mat4
make_scale(float x, float y, float z)
{
    return glm::scale(glm::vec3(x, y, z));
}

// Main module
 
PYBIND11_MODULE(ospray, m) 
{
    m.doc() = "OSPRay bindings";
    
    define_enums(m);
    
    m.def("init", &init);
    /*
    m.def("init", [&](const std::vector<std::string>& args) {
        return init(m, args);
        //return py::make_tuple();
    });
    */
    
    m.def("shutdown", &ospShutdown);
        
    declare_managedobject<ManagedCamera>(m, "ManagedCamera");
    declare_managedobject<ManagedData>(m, "ManagedData");
    declare_managedobject<ManagedFrameBuffer>(m, "ManagedFrameBuffer");
    declare_managedobject<ManagedFuture>(m, "ManagedFuture");
    declare_managedobject<ManagedGeometricModel>(m, "ManagedGeometricModel");
    declare_managedobject<ManagedGeometry>(m, "ManagedGeometry");
    declare_managedobject<ManagedGroup>(m, "ManagedGroup");
    declare_managedobject<ManagedImageOperation>(m, "ManagedImageOperation");
    declare_managedobject<ManagedInstance>(m, "ManagedInstance");
    declare_managedobject<ManagedLight>(m, "ManagedLight");
    declare_managedobject<ManagedMaterial>(m, "ManagedMaterial");
    declare_managedobject<ManagedRenderer>(m, "ManagedRenderer");
    declare_managedobject<ManagedTexture>(m, "ManagedTexture");
    declare_managedobject<ManagedTransferFunction>(m, "ManagedTransferFunction");
    declare_managedobject<ManagedVolume>(m, "ManagedVolume");
    declare_managedobject<ManagedVolumetricModel>(m, "ManagedVolumetricModel");
    declare_managedobject<ManagedWorld>(m, "ManagedWorld");
    
    // Device
    
    m.def("get_current_device", []() {
        return ospray::cpp::Device(ospGetCurrentDevice());
    });
    // Current device only
    m.def("set_error_callback", [](py::function& func) { 
        py_error_callback = func; 
    });
    m.def("set_status_callback", [](py::function& func) { 
        py_status_callback = func; 
    });
    
    py::class_<ospray::cpp::Device>(m, "Device")
        .def(py::init<const std::string &>(), py::arg("type")="default")
        //.def("handle", &ospray::cpp::Device::handle)      // Leads to incomplete type 'osp::Device' used in type trait expression
        .def("commit", &ospray::cpp::Device::commit)
        .def("set_param", (void (ospray::cpp::Device::*)(const std::string &, const std::string &) const) &ospray::cpp::Device::setParam)
        .def("set_param", &ospray::cpp::Device::setParam<bool>)
        .def("set_param", &ospray::cpp::Device::setParam<int>)
        //void set(const std::string &name, void *v) const;
    ;
    
    // Modules
    
    m.def("load_module", &load_module);
    
    // Scene
    
    py::class_<ospray::cpp::Camera, ManagedCamera>(m, "Camera")
        .def(py::init<const std::string &>())
    ;

    py::class_<ospray::cpp::CopiedData, ManagedData>(m, "CopiedData")
        .def(py::init<const ospray::cpp::GeometricModel &>())
        .def(py::init<const ospray::cpp::Geometry &>())
        .def(py::init<const ospray::cpp::ImageOperation &>())
        .def(py::init<const ospray::cpp::Instance &>())
        .def(py::init<const ospray::cpp::Light &>())
        .def(py::init<const ospray::cpp::VolumetricModel &>())
        .def(py::init(
            [](py::array& array) {
                return copied_data_from_numpy_array(array);
            }))
    ;

    py::class_<ospray::cpp::SharedData, ManagedData>(m, "SharedData")
        .def(py::init<const ospray::cpp::GeometricModel &>())
        .def(py::init<const ospray::cpp::Geometry &>())
        .def(py::init<const ospray::cpp::ImageOperation &>())
        .def(py::init<const ospray::cpp::Instance &>())
        .def(py::init<const ospray::cpp::Light &>())
        .def(py::init<const ospray::cpp::VolumetricModel &>())
        .def(py::init(
            [](py::array& array) {
                return shared_data_from_numpy_array(array);
            }))
    ;
            
    py::class_<ospray::cpp::PickResult>(m, "PickResult")
        .def_readonly("has_hit", &ospray::cpp::PickResult::hasHit)
        .def_property_readonly("instance", 
            [](const ospray::cpp::PickResult& self) {
                return ospray::cpp::Instance(self.instance);
            })
        .def_property_readonly("model", 
            [](const ospray::cpp::PickResult& self) {
                return ospray::cpp::GeometricModel(self.model);
            })                        
        .def_readonly("prim_id", &ospray::cpp::PickResult::primID)   
        .def_readonly("world_position", &ospray::cpp::PickResult::worldPosition)    
    ;
            
    py::class_<ospray::cpp::FrameBuffer, ManagedFrameBuffer>(m, "FrameBuffer")
        .def(py::init<int, int, OSPFrameBufferFormat, int>(),
            py::arg(), py::arg(), py::arg("format")=OSP_FB_SRGBA, py::arg("channels")=OSP_FB_COLOR)
        .def("clear", &ospray::cpp::FrameBuffer::clear)
        .def("get_variance", [](const ospray::cpp::FrameBuffer& self) {
                return ospGetVariance(self.handle());
            })
        .def("get", &framebuffer_get, py::arg(), py::arg(), py::arg("format")=OSP_FB_NONE)        
        .def("pick", &ospray::cpp::FrameBuffer::pick)
        .def("render_frame", &ospray::cpp::FrameBuffer::renderFrame)
        .def("reset_accumulation", &ospray::cpp::FrameBuffer::resetAccumulation)
    ;
       
    py::class_<ospray::cpp::Future, ManagedFuture>(m, "Future")
        .def(py::init<>())
        .def("cancel", &ospray::cpp::Future::cancel)
        .def("is_ready", &ospray::cpp::Future::isReady, py::arg("event")=OSP_TASK_FINISHED)
        .def("progress", &ospray::cpp::Future::progress)
        .def("wait", &ospray::cpp::Future::wait, py::arg("event")=OSP_TASK_FINISHED)
    ;            
            
    py::class_<ospray::cpp::GeometricModel, ManagedGeometricModel>(m, "GeometricModel")
        .def(py::init<const ospray::cpp::Geometry &>())
    ;
    
    py::class_<ospray::cpp::Geometry, ManagedGeometry>(m, "Geometry")
        .def(py::init<const std::string &>())
    ;
    
    py::class_<ospray::cpp::Group, ManagedGroup>(m, "Group")
        .def(py::init<>())
    ;
    
    py::class_<ospray::cpp::ImageOperation, ManagedImageOperation>(m, "ImageOperation")
        .def(py::init<const std::string &>())
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

    py::class_<ospray::cpp::Texture, ManagedTexture>(m, "Texture")
        .def(py::init<const std::string &>())
    ;

    py::class_<ospray::cpp::TransferFunction, ManagedTransferFunction>(m, "TransferFunction")
        .def(py::init<const std::string &>())
    ;

    py::class_<ospray::cpp::Volume, ManagedVolume>(m, "Volume")
        .def(py::init<const std::string &>())
    ;

    py::class_<ospray::cpp::VolumetricModel, ManagedVolumetricModel>(m, "VolumetricModel")
        .def(py::init<const ospray::cpp::Volume &>())
    ;

    py::class_<ospray::cpp::World, ManagedWorld>(m, "World")
        .def(py::init<>())
    ;
    
    // Utility
    
    py::class_<glm::mat4>(m, "mat4")      
        // Static
        .def_static("identity", &make_identity)
        .def_static("scale", &make_scale)
        .def_static("translate", &make_translate)
        .def_static("rotate", &make_rotate)   
        .def_static("from_quaternion", &make_from_quaternion)
        // Member     
        .def(py::init<>())
        .def("__repr__", print_mat4)
        .def(-py::self)        
        .def(float() * py::self)
        //.def(py::self / float())
        //.def(py::self /= float())
        //.def(py::self *= py::self())
        //.def(py::self *= float())
        .def(py::self + py::self)
        .def(py::self - py::self)
        .def(py::self * py::self)
        .def(py::self / py::self)
    ;
    
    m.def("copied_data_constructor", &copied_data_from_numpy_array, py::arg());
    m.def("copied_data_constructor_vec", &copied_data_from_numpy_array_vec, py::arg());
    m.def("copied_data_constructor_box", &copied_data_from_numpy_array_box, py::arg());

    m.def("shared_data_constructor", &shared_data_from_numpy_array, py::arg());
    m.def("shared_data_constructor_vec", &shared_data_from_numpy_array_vec, py::arg());
    m.def("shared_data_constructor_box", &shared_data_from_numpy_array_box, py::arg());
    
    // Library version

    // Compile-time
    m.attr("VERSION") = py::make_tuple(OSPRAY_VERSION_MAJOR, OSPRAY_VERSION_MINOR, OSPRAY_VERSION_PATCH);
    
    // Run-time, can only be called after a device is created. 
    // Returns version for current device
    m.def("version", &runtime_version);

    // Define testing submodule
    // Usage of ospray_testing unfortunately isn't easy to provide for a binary build, see https://github.com/ospray/ospray/issues/419
    //define_testing(m);
}

