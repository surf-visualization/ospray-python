#ifndef CONVERSION_H
#define CONVERSION_H

#include <pybind11/pybind11.h>
#include "vec.h"

namespace pybind11 { namespace detail {
    
    // vec234f
    
    template <> struct type_caster<vec2f> {
    public:
        PYBIND11_TYPE_CASTER(vec2f, _("vec2f"));

        // Conversion part 1 (Python -> C++)
        bool load(handle src, bool) 
        {
            float x, y;
            
            PyObject *source = src.ptr();
            
            if (!PyArg_ParseTuple(source, "ff", &x, &y))
                return false;
            
            value.x = x;
            value.y = y;
            
            return (!PyErr_Occurred());
        }

        // Conversion part 2 (C++ -> Python)
        static handle cast(const vec2f& src, return_value_policy /* policy */, handle /* parent */) 
        {
            return Py_BuildValue("ff", src.x, src.y);
        }
    };
        
    template <> struct type_caster<vec3f> {
    public:        
        PYBIND11_TYPE_CASTER(vec3f, _("vec3f"));

        // Conversion part 1 (Python -> C++)
        bool load(handle src, bool) 
        {
            float x, y, z;
            
            /* Extract PyObject from handle */
            PyObject *source = src.ptr();
            
            if (!PyArg_ParseTuple(source, "fff", &x, &y, &z))
                return false;
            
            value.x = x;
            value.y = y;
            value.z = z;
            
            return (!PyErr_Occurred());
        }

        // Conversion part 2 (C++ -> Python)
        static handle cast(const vec3f& src, return_value_policy /* policy */, handle /* parent */) 
        {
            return Py_BuildValue("fff", src.x, src.y, src.z);
        }
    };

    template <> struct type_caster<vec4f> {
    public:        
        PYBIND11_TYPE_CASTER(vec4f, _("vec4f"));

        // Conversion part 1 (Python -> C++)
        bool load(handle src, bool) 
        {
            float x, y, z, w;
            
            /* Extract PyObject from handle */
            PyObject *source = src.ptr();
            
            if (!PyArg_ParseTuple(source, "ffff", &x, &y, &z, &w))
                return false;
            
            value.x = x;
            value.y = y;
            value.z = z;
            value.w = w;
            
            return (!PyErr_Occurred());
        }

        // Conversion part 2 (C++ -> Python)
        static handle cast(const vec4f& src, return_value_policy /* policy */, handle /* parent */) 
        {
            return Py_BuildValue("ffff", src.x, src.y, src.z, src.w);
        }
    };

    // vec234i
    
    template <> struct type_caster<vec2i> {
    public:
        PYBIND11_TYPE_CASTER(vec2i, _("vec2i"));

        // Conversion part 1 (Python -> C++)
        bool load(handle src, bool) 
        {
            int x, y;
            
            PyObject *source = src.ptr();
            
            if (!PyArg_ParseTuple(source, "ii", &x, &y))
                return false;
            
            value.x = x;
            value.y = y;
            
            return (!PyErr_Occurred());
        }

        // Conversion part 2 (C++ -> Python)
        static handle cast(const vec2i& src, return_value_policy /* policy */, handle /* parent */) 
        {
            return Py_BuildValue("ii", src.x, src.y);
        }
    };
        
    template <> struct type_caster<vec3i> {
    public:        
        PYBIND11_TYPE_CASTER(vec3i, _("vec3i"));

        // Conversion part 1 (Python -> C++)
        bool load(handle src, bool) 
        {
            int x, y, z;
            
            /* Extract PyObject from handle */
            PyObject *source = src.ptr();
            
            if (!PyArg_ParseTuple(source, "iii", &x, &y, &z))
                return false;
            
            value.x = x;
            value.y = y;
            value.z = z;
            
            return (!PyErr_Occurred());
        }

        // Conversion part 2 (C++ -> Python)
        static handle cast(const vec3i& src, return_value_policy /* policy */, handle /* parent */) 
        {
            return Py_BuildValue("iii", src.x, src.y, src.z);
        }
    };

    template <> struct type_caster<vec4i> {
    public:        
        PYBIND11_TYPE_CASTER(vec4i, _("vec4i"));

        // Conversion part 1 (Python -> C++)
        bool load(handle src, bool) 
        {
            float x, y, z, w;
            
            /* Extract PyObject from handle */
            PyObject *source = src.ptr();
            
            if (!PyArg_ParseTuple(source, "iiii", &x, &y, &z, &w))
                return false;
            
            value.x = x;
            value.y = y;
            value.z = z;
            value.w = w;
            
            return (!PyErr_Occurred());
        }

        // Conversion part 2 (C++ -> Python)
        static handle cast(const vec4i& src, return_value_policy /* policy */, handle /* parent */) 
        {
            return Py_BuildValue("iiii", src.x, src.y, src.z, src.w);
        }
    };

}} // namespace pybind11::detail

#endif
