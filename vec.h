#ifndef VEC_H
#define VEC_H

#include <ospray/ospray_cpp.h>

// vec[234]f

struct vec2f { 
    float x, y;

    vec2f(): x(0), y(0) {}
    vec2f(float xx, float yy): x(xx), y(yy) {}
};

struct vec3f { 
    float x, y, z; 

    vec3f(): x(0), y(0), z(0) {}
    vec3f(float xx, float yy, float zz): x(xx), y(yy), z(zz) {}
};

struct vec4f { 
    float x, y, z, w; 

    vec4f(): x(0), y(0), z(0), w(0) {}
    vec4f(float xx, float yy, float zz, float ww): x(xx), y(yy), z(zz), w(ww) {}
};

// vec[234]i

struct vec2i { 
    int32_t x, y;

    vec2i(): x(0), y(0) {}
    vec2i(int32_t xx, int32_t yy): x(xx), y(yy) {}
};

struct vec3i { 
    int32_t x, y, z; 

    vec3i(): x(0), y(0), z(0) {}
    vec3i(int32_t xx, int32_t yy, int32_t zz): x(xx), y(yy), z(zz) {}
};

struct vec4i { 
    int32_t x, y, z, w; 

    vec4i(): x(0), y(0), z(0), w(0) {}
    vec4i(int32_t xx, int32_t yy, int32_t zz, int32_t ww): x(xx), y(yy), z(zz), w(ww) {}
};

// vec[234]l

struct vec2l { 
    int64_t x, y; 

    vec2l(): x(0), y(0) {}
    vec2l(int64_t xx, int64_t yy): x(xx), y(yy) {}
};

struct vec3l { 
    int64_t x, y, z; 

    vec3l(): x(0), y(0), z(0) {}
    vec3l(int64_t xx, int64_t yy, int64_t zz): x(xx), y(yy), z(zz) {}
};

struct vec4l { 
    int64_t x, y, z, w; 

    vec4l(): x(0), y(0), z(0), w(0) {}
    vec4l(int64_t xx, int64_t yy, int64_t zz, int64_t ww): x(xx), y(yy), z(zz), w(ww) {}
};


// vec[234]ui

struct vec2ui { 
    uint32_t x, y; 

    vec2ui(): x(0), y(0) {}
    vec2ui(uint32_t xx, uint32_t yy): x(xx), y(yy) {}
};

struct vec3ui { 
    uint32_t x, y, z; 

    vec3ui(): x(0), y(0), z(0) {}
    vec3ui(uint32_t xx, uint32_t yy, uint32_t zz): x(xx), y(yy), z(zz) {}
};

struct vec4ui { 
    uint32_t x, y, z, w; 

    vec4ui(): x(0), y(0), z(0), w(0) {}    
    vec4ui(uint32_t xx, uint32_t yy, uint32_t zz, uint32_t ww): x(xx), y(yy), z(zz), w(ww) {}
};

// vec[234]ul

struct vec2ul { 
    uint64_t x, y; 

    vec2ul(): x(0), y(0) {}
    vec2ul(uint64_t xx, uint64_t yy): x(xx), y(yy) {}
};

struct vec3ul { 
    uint64_t x, y, z; 

    vec3ul(): x(0), y(0), z(0) {}
    vec3ul(uint64_t xx, uint64_t yy, uint64_t zz): x(xx), y(yy), z(zz) {}
};

struct vec4ul { 
    uint64_t x, y, z, w; 

    vec4ul(): x(0), y(0), z(0), w(0) {}    
    vec4ul(uint64_t xx, uint64_t yy, uint64_t zz, uint64_t ww): x(xx), y(yy), z(zz), w(ww) {}
};

// vec[234]uc

struct vec2uc { 
    unsigned char x, y; 

    vec2uc(): x(0), y(0) {}
    vec2uc(unsigned char xx, unsigned char yy): x(xx), y(yy) {}
};

struct vec3uc { 
    unsigned char x, y, z; 

    vec3uc(): x(0), y(0), z(0) {}
    vec3uc(unsigned char xx, unsigned char yy, unsigned char zz): x(xx), y(yy), z(zz) {}
};

struct vec4uc { 
    unsigned char x, y, z, w; 

    vec4uc(): x(0), y(0), z(0), w(0) {}
    vec4uc(unsigned char xx, unsigned char yy, unsigned char zz, unsigned char ww): x(xx), y(yy), z(zz), w(ww) {}
};

// box3f

struct box3f {

    vec3f   lower, upper;

};

namespace ospray {
OSPTYPEFOR_SPECIALIZATION(vec2f, OSP_VEC2F);
OSPTYPEFOR_SPECIALIZATION(vec3f, OSP_VEC3F);
OSPTYPEFOR_SPECIALIZATION(vec4f, OSP_VEC4F);

OSPTYPEFOR_SPECIALIZATION(vec2i, OSP_VEC2I);
OSPTYPEFOR_SPECIALIZATION(vec3i, OSP_VEC3I);
OSPTYPEFOR_SPECIALIZATION(vec4i, OSP_VEC4I);

OSPTYPEFOR_SPECIALIZATION(vec2l, OSP_VEC2L);
OSPTYPEFOR_SPECIALIZATION(vec3l, OSP_VEC3L);
OSPTYPEFOR_SPECIALIZATION(vec4l, OSP_VEC4L);

OSPTYPEFOR_SPECIALIZATION(vec2uc, OSP_VEC2UC);
OSPTYPEFOR_SPECIALIZATION(vec3uc, OSP_VEC3UC);
OSPTYPEFOR_SPECIALIZATION(vec4uc, OSP_VEC4UC);

OSPTYPEFOR_SPECIALIZATION(vec2ui, OSP_VEC2UI);
OSPTYPEFOR_SPECIALIZATION(vec3ui, OSP_VEC3UI);
OSPTYPEFOR_SPECIALIZATION(vec4ui, OSP_VEC4UI);

OSPTYPEFOR_SPECIALIZATION(vec2ul, OSP_VEC2UL);
OSPTYPEFOR_SPECIALIZATION(vec3ul, OSP_VEC3UL);
OSPTYPEFOR_SPECIALIZATION(vec4ul, OSP_VEC4UL);

OSPTYPEFOR_SPECIALIZATION(box3f, OSP_BOX3F);

}

#endif
