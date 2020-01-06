#ifndef TESTING_H
#define TESTING_H

#include <pybind11/pybind11.h>
#include <ospray/ospray_testing/ospray_testing.h>

namespace py = pybind11;

struct SceneBuilder
{
    SceneBuilder(const std::string& scene)
    {
        handle = ospray::testing::newBuilder(scene);
    }
    
    ~SceneBuilder()
    {
        ospray::testing::release(handle);
    }
    
    void 
    commit()
    {
        ospray::testing::commit(handle);
    }
    
    ospray::cpp::Group
    build_group()
    {
        return ospray::testing::buildGroup(handle);
    }

    ospray::cpp::World
    build_world()
    {
        return ospray::testing::buildWorld(handle);
    }
    
    void
    set_param(const std::string& type, py::object value)
    {
        const std::string vtype = value.get_type().attr("__name__").cast<std::string>();
        
        if (vtype == "bool")
            ospray::testing::setParam(handle, type, value.cast<bool>());
        else if (vtype == "int")
            ospray::testing::setParam(handle, type, value.cast<int>());
        else if (vtype == "str")
            ospray::testing::setParam(handle, type, value.cast<std::string>());
        else
            printf("Warning: unhandled type '%s' in SceneBuilder.set_param('%s', ...)\n", vtype.c_str(), type.c_str());
    }
    
    ospray::testing::SceneBuilderHandle handle;
};

inline void 
define_testing(py::module& m)
{
    py::module t = m.def_submodule("testing", "ospray_testing");
    
    py::class_<SceneBuilder>(t, "SceneBuilder")
        .def(py::init<const std::string &>())
        .def("build_group", &SceneBuilder::build_group)
        .def("build_world", &SceneBuilder::build_world)
        .def("set_param", &SceneBuilder::set_param)
        .def("commit", &SceneBuilder::commit)
    ;
    
    /*
    auto builder = testing::newBuilder(scene);
    testing::setParam(builder, "rendererType", rendererTypeStr);
    if (scene == "curves") {
    testing::setParam(builder, "curveBasis", curveBasis);
    }
    testing::commit(builder);

    world = testing::buildWorld(builder);
    testing::release(builder);
    */
}

#endif
