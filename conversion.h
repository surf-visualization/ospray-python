#ifndef CONVERSION_H
#define CONVERSION_H

#include <pybind11/pybind11.h>
#include "ospcommon/math/AffineSpace.h"

namespace pybind11 { namespace detail {
    template <> struct type_caster<ospcommon::math::vec3f> {
    public:
        /**
         * This macro establishes the name 'inty' in
         * function signatures and declares a local variable
         * 'value' of type inty
         */
        PYBIND11_TYPE_CASTER(ospcommon::math::vec3f, _("ospcommon::math::vec3f"));

        /**
         * Conversion part 1 (Python->C++): convert a PyObject into an
         * instance or return false upon failure. The second argument
         * indicates whether implicit conversions should be applied.
         */
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

        /**
         * Conversion part 2 (C++ -> Python): convert an instance into
         * a Python object. The second and third arguments are used to
         * indicate the return value policy and parent object (for
         * ``return_value_policy::reference_internal``) and are generally
         * ignored by implicit casters.
         */
        static handle cast(const ospcommon::math::vec3f& src, return_value_policy /* policy */, handle /* parent */) 
        {
            return Py_BuildValue("fff", src.x, src.y, src.z);
        }
    };
}} // namespace pybind11::detail

#endif
