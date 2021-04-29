#include "Shader.h"

#include <string>
#include <cassert>

namespace Shader
{

GLuint CompileShader(const GLchar* src, GLenum type)
{
    assert(src);

    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, NULL);

    glCompileShader(shader);

    GLint compiled;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);

    if (!compiled)
    {
        GLint info_length = 0;

        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &info_length);

        std::string info_log;
        info_log.resize(info_length);

        glGetShaderInfoLog(shader, info_length, NULL, (GLchar*)(&info_log[0]));
        fprintf(stderr, "Shader compilation failed with log:\n%s\n", info_log.c_str());

        glDeleteShader(shader);

        return 0;
    }

    return shader;
}

bool CompileProgram(const GLchar* vertex, const GLchar* fragment, GLuint* program_id, const char** attributes, int attribute_count)
{
    GLuint vertex_id = CompileShader(vertex, GL_VERTEX_SHADER);
    assert(vertex_id && "Vertex shader compilation failed");

    GLuint fragment_id = CompileShader(fragment, GL_FRAGMENT_SHADER);
    assert(fragment_id && "Fragment shader compilation failed");

    *program_id = glCreateProgram();

    glAttachShader(*program_id, vertex_id);
    glAttachShader(*program_id, fragment_id);

    for (int i = 0; i < attribute_count; i++)
    {
        glBindAttribLocation(*program_id, i, attributes[i]);
    }

    glLinkProgram(*program_id);

    GLint linked;
    glGetProgramiv(*program_id, GL_LINK_STATUS, &linked);

    if (!linked)
    {
        GLint info_length = 0;

        glGetProgramiv(*program_id, GL_INFO_LOG_LENGTH, &info_length);

        std::string info_log;
        info_log.resize(info_length);

        glGetProgramInfoLog(*program_id, info_length, NULL, (GLchar*)(&info_log[0]));
        fprintf(stderr, "Linking program:\n%s\n", &info_log[0]);

        return false;
    }

    glDeleteShader(vertex_id);
    glDeleteShader(fragment_id);
    return true;
}

Program CompileDefaultProgram()
{
    Program program;

    const GLchar* vertexShaderSrc = (GLchar*)R"END(
        #version 150
        in vec2 position;
        in vec2 uv;
        out vec2 f_uv;
        void main(void)
        {
            f_uv = vec2(1.0-uv.x, uv.y);
            gl_Position = vec4(position, 0.0, 1.0);
        }
    )END";

    const GLchar* fragmentShaderSrc = (GLchar*)R"END(
        #version 150
        out vec4 fragColor;
        in vec2 f_uv;
        uniform sampler2D u_texture;
        uniform sampler2D u_textureReference;
        uniform sampler2D u_textureGrain;
        uniform vec2 u_textureResolution;
        uniform float u_hue;
        uniform float u_saturation;
        uniform float u_lightness;
        uniform float u_LUTStrength;
        uniform float u_vignetteStrength;
        uniform float u_grainStrength;
        uniform float u_brightness;
        uniform float u_contrast;
        // Grain resolution is constant size.
        const vec2 GrainResolution = vec2(1920.0, 1080.0);
        vec3 rgb2hsv(vec3 c)
        {
            vec4 K = vec4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);
            vec4 p = mix(vec4(c.bg, K.wz), vec4(c.gb, K.xy), step(c.b, c.g));
            vec4 q = mix(vec4(p.xyw, c.r), vec4(c.r, p.yzx), step(p.x, c.r));
            float d = q.x - min(q.w, q.y);
            float e = 1.0e-10;
            return vec3(abs(q.z + (q.w - q.y) / (6.0 * d + e)), d / (q.x + e), q.x);
        }
        vec3 hsv2rgb(vec3 c)
        {
            vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
            vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
            return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
        }
        vec3 blendOverlay(vec3 base, vec3 blend)
        {
            return mix(1.0 - 2.0 * (1.0 - base) * (1.0 - blend), 2.0 * base * blend, step(base, vec3(0.5)));
        }
        float srgb2linearf(float v)
        {
            if (v <= 0.04045) return v / 12.92;
            return pow((v + 0.055) / 1.055, 2.4);
        }
        vec3 srgb2linear(vec3 color)
        {
            return vec3(
                srgb2linearf(color.r),
                srgb2linearf(color.g),
                srgb2linearf(color.b));
        }
        float linear2srgbf(float v)
        {
            if (v <= 0.0031308) return 12.92 * v;
            return pow(v, 1.0 / 2.4) * 1.055 - 0.055;
        }
        vec3 linear2srgb(vec3 color)
        {
            return vec3(
                linear2srgbf(color.r),
                linear2srgbf(color.g),
                linear2srgbf(color.b));
        }
        void main(void)
        {
            vec3 textureLUT = srgb2linear(texture(u_texture, f_uv).rgb);
            vec3 textureRef = srgb2linear(texture(u_textureReference, f_uv).rgb);
            // Round trip for HSV adjustment
            textureRef = hsv2rgb(rgb2hsv(textureRef) * vec3(u_hue, u_saturation, u_lightness));
            // LUT mix
            vec3 color = mix(textureRef, textureLUT, u_LUTStrength);
            // Contrast/Brightness adjustment
            color = color * u_contrast + (0.5 - u_contrast * 0.5) + u_brightness;
            // Grain blend overlay
            vec3 grain = texture(u_textureGrain, (f_uv * u_textureResolution) / GrainResolution).rgb;
            color = mix(color, blendOverlay(color, grain), u_grainStrength);
            // Vignetting
            vec2 uv = f_uv * (1.0 - f_uv);
            // TODO: extract these constants
            float vignette = pow(uv.x * uv.y * 15.0, 0.15);
            color = mix(color, color * vignette, u_vignetteStrength);
            fragColor = vec4(linear2srgb(color), 1.0);
        }
    )END";

    const char* attributes[] =
    {
        "position",
        "uv",
    };

    CompileProgram(vertexShaderSrc, fragmentShaderSrc, &program.ID, attributes, 2);

    glBindAttribLocation(program.ID, 0, "position");
    glBindAttribLocation(program.ID, 1, "uv");

    const float vertices[] =
    {
        -1.0f,  1.0f, 1.0f, 0.0f,
         1.0f,  1.0f, 0.0f, 0.0f,
         1.0f, -1.0f, 0.0f, 1.0f,
        -1.0f, -1.0f, 1.0f, 1.0f,
        -1.0f,  1.0f, 1.0f, 0.0f,
         1.0f, -1.0f, 0.0f, 1.0f,
    };

    program.VertexCount = 6;
    program.Texture = glGetUniformLocation(program.ID, "u_texture");
    program.TextureReference = glGetUniformLocation(program.ID, "u_textureReference");
    program.TextureGrain = glGetUniformLocation(program.ID, "u_textureGrain");
    program.TextureResolution = glGetUniformLocation(program.ID, "u_textureResolution");
    program.LUTStrength = glGetUniformLocation(program.ID, "u_LUTStrength");
    program.VignetteStrength = glGetUniformLocation(program.ID, "u_vignetteStrength");
    program.GrainStrength = glGetUniformLocation(program.ID, "u_grainStrength");
    program.Hue = glGetUniformLocation(program.ID, "u_hue");
    program.Saturation = glGetUniformLocation(program.ID, "u_saturation");
    program.Lightness = glGetUniformLocation(program.ID, "u_lightness");
    program.Brightness = glGetUniformLocation(program.ID, "u_brightness");
    program.Contrast = glGetUniformLocation(program.ID, "u_contrast");

    glGenVertexArrays(1, &program.VAO);
    glBindVertexArray(program.VAO);
    glGenBuffers(1, &program.VBO);
    glBindBuffer(GL_ARRAY_BUFFER, program.VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

    return program;
}

}
