#ifndef CONVERSION_H
#define CONVERSION_H

#include <pybind11/pybind11.h>
#include "ospcommon/math/AffineSpace.h"

namespace pybind11 { namespace detail {
    
    template <> struct type_caster<ospcommon::math::vec2f> {
    public:
        PYBIND11_TYPE_CASTER(ospcommon::math::vec2f, _("ospcommon::math::vec2f"));

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
        static handle cast(const ospcommon::math::vec2f& src, return_value_policy /* policy */, handle /* parent */) 
        {
            return Py_BuildValue("ff", src.x, src.y);
        }
    };
        
    template <> struct type_caster<ospcommon::math::vec3f> {
    public:        
        PYBIND11_TYPE_CASTER(ospcommon::math::vec3f, _("ospcommon::math::vec3f"));

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
        static handle cast(const ospcommon::math::vec3f& src, return_value_policy /* policy */, handle /* parent */) 
        {
            return Py_BuildValue("fff", src.x, src.y, src.z);
        }
    };

    template <> struct type_caster<ospcommon::math::vec4f> {
    public:        
        PYBIND11_TYPE_CASTER(ospcommon::math::vec4f, _("ospcommon::math::vec4f"));

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
        static handle cast(const ospcommon::math::vec4f& src, return_value_policy /* policy */, handle /* parent */) 
        {
            return Py_BuildValue("ffff", src.x, src.y, src.z, src.w);
        }
    };

}} // namespace pybind11::detail

#endif
