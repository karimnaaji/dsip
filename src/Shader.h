#pragma once

#include "glad/glad.h"

namespace Shader {

struct Program
{
    unsigned int VertexCount;
    GLuint VAO;
    GLuint VBO;
    GLuint ID;
    GLuint Texture;
    GLuint TextureReference;
    GLuint TextureGrain;
    GLuint TextureResolution;
    GLuint LUTStrength;
    GLuint GrainStrength;
    GLuint VignetteStrength;
    GLuint Hue;
    GLuint Saturation;
    GLuint Lightness;
    GLuint Brightness;
    GLuint Contrast;
};

Program CompileDefaultProgram();

}
