#ifndef MAT_H
#define MAT_H

#include <string>
#include <glm/mat4x4.hpp>
#include <glm/gtc/type_ptr.hpp>

void
affine3fv_from_mat4(float *xform, const glm::mat4 &mat)
{
    const float *M = glm::value_ptr(mat);

    xform[0] = M[0];
    xform[1] = M[1];
    xform[2] = M[2];

    xform[3] = M[4];
    xform[4] = M[5];
    xform[5] = M[6];

    xform[6] = M[8];
    xform[7] = M[9];
    xform[8] = M[10];

    xform[9] = M[12];
    xform[10] = M[13];
    xform[11] = M[14];
}

std::string
print_mat4(const glm::mat4& mat)
{
    const float *M = glm::value_ptr(mat);

    std::string s = "";
    char t[256];

    // Print in mathematical convention
    for (int i = 0; i < 4; i++)
    {
        for (int j = 0; j < 4; j++)
        {
            sprintf(t, "%.6f ", M[4*j+i]);
            s += t;
        }
        s += "\n";
    }

    return s;
}


#endif
