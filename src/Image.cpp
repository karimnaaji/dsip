#include "Image.h"
#include "Util.h"
#include "FilmGrain.h"
#include "LUTs.h"

#include <cassert>
#include <cmath>
#include <fstream>
#include <algorithm>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

namespace Image
{

ImageDesc::ImageDesc()
{
    std::memset(this, 0x0, sizeof(ImageDesc));
}

ImageDesc::~ImageDesc()
{
    delete[] ScratchData;
}

ImageData::ImageData()
{
    std::memset(this, 0x0, sizeof(ImageData));
}

HistogramDesc::HistogramDesc(unsigned int width, unsigned int height)
    : MaxValue(0.0f)
    , Width(width)
    , Height(height)
{
    Allocate();
}

HistogramDesc::~HistogramDesc()
{
    Free();
}

void HistogramDesc::Allocate()
{
    Red = new uint8_t[Width * Height];
    Green = new uint8_t[Width * Height];
    Blue = new uint8_t[Width * Height];

    HistogramRed = new float[Width * Height] {0};
    HistogramGreen = new float[Width * Height] {0};
    HistogramBlue = new float[Width * Height] {0};
}

void HistogramDesc::Free()
{
    delete[] Red;
    delete[] Green;
    delete[] Blue;
    delete[] HistogramRed;
    delete[] HistogramGreen;
    delete[] HistogramBlue;
}

#ifdef DSIP_GUI
void CaptureHistogram(HistogramDesc& histogram, unsigned int width, unsigned int height)
{
    if (width != histogram.Width || height != histogram.Height)
    {
        histogram.Width = width;
        histogram.Height = height;
        histogram.Free();
        histogram.Allocate();
    }

    histogram.MaxValue = 0.0f;

    glReadPixels(0, 0, width, height, GL_RED, GL_UNSIGNED_BYTE, histogram.Red);
    glReadPixels(0, 0, width, height, GL_GREEN, GL_UNSIGNED_BYTE, histogram.Green);
    glReadPixels(0, 0, width, height, GL_BLUE, GL_UNSIGNED_BYTE, histogram.Blue);

    unsigned int size = width * height;
    float size_inverse = 1.0f / size;

    std::memset(histogram.HistogramRed, 0x0, sizeof(float) * size);
    std::memset(histogram.HistogramGreen, 0x0, sizeof(float) * size);
    std::memset(histogram.HistogramBlue, 0x0, sizeof(float) * size);

    for (int i = 0; i < size; ++i)
    {
        histogram.HistogramRed[histogram.Red[i]] += size_inverse;
        histogram.MaxValue = std::max(histogram.MaxValue, histogram.HistogramRed[histogram.Red[i]]);
    }
    for (int i = 0; i < size; ++i)
    {
        histogram.HistogramGreen[histogram.Green[i]] += size_inverse;
        histogram.MaxValue = std::max(histogram.MaxValue, histogram.HistogramGreen[histogram.Green[i]]);
    }
    for (int i = 0; i < size; ++i)
    {
        histogram.HistogramBlue[histogram.Blue[i]] += size_inverse;
        histogram.MaxValue = std::max(histogram.MaxValue, histogram.HistogramBlue[histogram.Blue[i]]);
    }
}
#endif

float SRGB2Linear(float c)
{
    if (c <= 0.04045f)
    {
        return c / 12.92f;
    }
    return powf((c + 0.055f) / 1.055f, 2.4f);
}

float Linear2SRGB(float c)
{
    if (c <= 0.0031308f)
    {
        return 12.92f * c;
    }
    return powf(c, 1.0f / 2.4f) * 1.055f - 0.055f;
}

float Clamp(float v, float min, float max)
{
    return v <= min ? min : v >= max ? max : v;
}

float Mix(float f0, float f1, float v)
{
    return f0 * v + f1 * (1.0f - v);
}

void HSV2RGB(const float* hsv, float* output)
{
    float hue = hsv[0];
    float saturation = hsv[1];
    float lightness = hsv[2];

    if (saturation == 0.0f)
    {
        output[0] = output[1] = output[2] = lightness;
        return;
    }

    float h = hue / 60.0f;
    int i = (int)h;
    float frac = h - i;
    float p = lightness * (1.0f - saturation);
    float q = lightness * (1.0f - saturation * frac);
    float t = lightness * (1.0f - saturation * (1.0f - frac));

    switch(i)
    {
        case 0:
            output[0] = lightness;
            output[1] = t;
            output[2] = p;
            break;
        case 1:
            output[0] = q;
            output[1] = lightness;
            output[2] = p;
            break;
        case 2:
            output[0] = p;
            output[1] = lightness;
            output[2] = t;
            break;
        case 3:
            output[0] = p;
            output[1] = q;
            output[2] = lightness;
            break;
        case 4:
            output[0] = t;
            output[1] = p;
            output[2] = lightness;
            break;
        case 5:
            output[0] = lightness;
            output[1] = p;
            output[2] = q;
            break;
        default:
            assert(false);
            break;
    }
}

void RGB2HSV(const float* input, float* hsv)
{
    float K = 0.0f;
    float r = input[0];
    float g = input[1];
    float b = input[2];

    if (g < b)
    {
        std::swap(g, b);
        K = -1.0f;
    }
    if (r < g)
    {
        std::swap(r, g);
        K = -2.0f / 6.0f - K;
    }
    float chroma = r - std::min(g, b);
    float hue = 360.0f * (K + (g - b) / (6.0f * chroma + 1e-20f));
    if (hue < 0)
    {
        hue = -hue;
    }
    float saturation = (chroma / (r + std::numeric_limits<float>::epsilon()));
    float lightness = r;

    hsv[0] = hue;
    hsv[1] = saturation;
    hsv[2] = lightness;
}

void ApplyVignette(const float* input, float* output, float* uv)
{
    uv[0] *= 1.0 - uv[0];
    uv[1] *= 1.0 - uv[1];

    float vignette = pow(uv[0] * uv[1] * 15.0f, 0.15);

    // TODO: dither (only on CPU pipeline)
    output[0] = input[0] * vignette;
    output[1] = input[1] * vignette;
    output[2] = input[2] * vignette;
}

void ApplyGrain(const float* input, float* output, float* grain)
{
    auto BlendOverlay = [](float base, float blend) -> float
    {
        float strength = base > 0.5 ? 0.0 : 1.0;
        return (1.0 - 2.0 * (1.0 - base) * (1.0 - blend)) * (1.0f - strength) + 2.0 * base * blend * strength;
    };

    output[0] = BlendOverlay(input[0], grain[0]);
    output[1] = BlendOverlay(input[1], grain[1]);
    output[2] = BlendOverlay(input[2], grain[2]);

#if 0
    float grain_strength = 0.25;
    out_rgb[0] = out_rgb[0] * (1.0f - grain_strength) + grain_r * grain_strength;
    out_rgb[1] = out_rgb[1] * (1.0f - grain_strength) + grain_g * grain_strength;
    out_rgb[2] = out_rgb[2] * (1.0f - grain_strength) + grain_b * grain_strength;
#endif
}

void ApplyLUT(const float* input, float* output, float* clut, unsigned int level)
{
    int color, red, green, blue, i, j;
    float tmp[6], r, g, b;
    level *= level;

    red = input[0] * (float)(level - 1);
    if(red > level - 2)
        red = (float)level - 2;
    if(red < 0)
        red = 0;

    green = input[1] * (float)(level - 1);
    if(green > level - 2)
        green = (float)level - 2;
    if(green < 0)
        green = 0;

    blue = input[2] * (float)(level - 1);
    if(blue > level - 2)
        blue = (float)level - 2;
    if(blue < 0)
        blue = 0;

    r = input[0] * (float)(level - 1) - red;
    g = input[1] * (float)(level - 1) - green;
    b = input[2] * (float)(level - 1) - blue;

    color = red + green * level + blue * level * level;

    i = color * 3;
    j = (color + 1) * 3;

    tmp[0] = clut[i++] * (1 - r) + clut[j++] * r;
    tmp[1] = clut[i++] * (1 - r) + clut[j++] * r;
    tmp[2] = clut[i] * (1 - r) + clut[j] * r;

    i = (color + level) * 3;
    j = (color + level + 1) * 3;

    tmp[3] = clut[i++] * (1 - r) + clut[j++] * r;
    tmp[4] = clut[i++] * (1 - r) + clut[j++] * r;
    tmp[5] = clut[i] * (1 - r) + clut[j] * r;

    output[0] = tmp[0] * (1 - g) + tmp[3] * g;
    output[1] = tmp[1] * (1 - g) + tmp[4] * g;
    output[2] = tmp[2] * (1 - g) + tmp[5] * g;

    i = (color + level * level) * 3;
    j = (color + level * level + 1) * 3;

    tmp[0] = clut[i++] * (1 - r) + clut[j++] * r;
    tmp[1] = clut[i++] * (1 - r) + clut[j++] * r;
    tmp[2] = clut[i] * (1 - r) + clut[j] * r;

    i = (color + level + level * level) * 3;
    j = (color + level + level * level + 1) * 3;

    tmp[3] = clut[i++] * (1 - r) + clut[j++] * r;
    tmp[4] = clut[i++] * (1 - r) + clut[j++] * r;
    tmp[5] = clut[i] * (1 - r) + clut[j] * r;

    tmp[0] = tmp[0] * (1 - g) + tmp[3] * g;
    tmp[1] = tmp[1] * (1 - g) + tmp[4] * g;
    tmp[2] = tmp[2] * (1 - g) + tmp[5] * g;

    output[0] = output[0] * (1 - b) + tmp[0] * b;
    output[1] = output[1] * (1 - b) + tmp[1] * b;
    output[2] = output[2] * (1 - b) + tmp[2] * b;
}

bool LoadImage(const char* path, ImageData& image)
{
    auto image_data = Util::BytesFromFile(path);

#ifdef DSIP_GUI
    if (image_data.size() == 0)
    {
        // Fallback to bundle
        image_data = Util::BytesFromBundle(path);
    }
#endif

    if (image_data.size() == 0) return false;

    const stbi_uc* stbi_data = reinterpret_cast<const stbi_uc*>(image_data.data());

    image.Pixels = stbi_load_from_memory(stbi_data, image_data.size(), &image.Width, &image.Height, &image.Comp, 0);

    if (!image.Pixels) return false;

    return true;
}

bool SaveImage(const char* path, const ImageDesc& image)
{
    stbi_write_png(path, image.Data.Width, image.Data.Height, image.Data.Comp, image.ScratchData, 0);

    return true;
}

void FreeImage(const ImageData& image_data)
{
    if (image_data.Pixels)
    {
        stbi_image_free(image_data.Pixels);
    }
}

bool SaveProfile(const char* path, const ProcessParams& process_params)
{
    std::ofstream file_profile(path, std::ios::out);

    if (!file_profile.is_open()) return false;

    file_profile << "lut_file_index:" << process_params.LUTIndex << std::endl;
    file_profile << "lut_strength:" << process_params.LUTStrength << std::endl;
    file_profile << "grain_strength:" << process_params.GrainStrength << std::endl;
    file_profile << "vignette_strength:" << process_params.VignetteStrength << std::endl;
    file_profile << "hue:" << process_params.Hue << std::endl;
    file_profile << "saturation:" << process_params.Saturation << std::endl;
    file_profile << "lightness:" << process_params.Lightness << std::endl;
    file_profile << "brightness:" << process_params.Brightness << std::endl;
    file_profile << "contrast:" << process_params.Contrast << std::endl;

    file_profile.close();

    return true;
}

bool LoadProfile(const char* path, ProcessParams& process_params)
{
    std::ifstream file_profile(path, std::ios::in);

    if (!file_profile.is_open()) return false;

    std::string line;

    const char eol = '\n';
    std::getline(file_profile, line, eol);
    std::sscanf(line.c_str(), "lut_file_index:%d", &process_params.LUTIndex);

    process_params.LUTFile = LUTs[process_params.LUTIndex];

    auto ReadParam = [&](const std::string& param_name, float* param_value)
    {
        std::getline(file_profile, line, eol);
        std::sscanf(line.c_str(), (param_name + ":%f").c_str(), param_value);
    };

    ReadParam("lut_strength", &process_params.LUTStrength);
    ReadParam("grain_strength", &process_params.GrainStrength);
    ReadParam("vignette_strength", &process_params.VignetteStrength);
    ReadParam("hue", &process_params.Hue);
    ReadParam("saturation", &process_params.Saturation);
    ReadParam("lightness", &process_params.Lightness);
    ReadParam("brightness", &process_params.Brightness);
    ReadParam("contrast", &process_params.Contrast);

    file_profile.close();

    return true;
}

void ProcessImage(ImageDesc& image, ProcessParams process_params)
{
    ImageData lut_image;
    ImageData grain_image;

    LoadImage(process_params.LUTFile, lut_image);
    LoadImage(process_params.GrainFile, grain_image);

    float* lut = new float[lut_image.Width * lut_image.Height * lut_image.Comp];

    for (int i = 0, lut_index = 0; i < lut_image.Height; ++i)
    {
        for (int j = 0; j < lut_image.Width; ++j)
        {
            int pixel_index = i * lut_image.Comp * lut_image.Width + j * lut_image.Comp;

            lut[lut_index++] = lut_image.Pixels[pixel_index + 0] / 255.0f;
            lut[lut_index++] = lut_image.Pixels[pixel_index + 1] / 255.0f;
            lut[lut_index++] = lut_image.Pixels[pixel_index + 2] / 255.0f;
        }
    }

    image.ScratchData = new uint8_t[image.Data.Width * image.Data.Height * image.Data.Comp];

    uint32_t film_grain_size = grain_image.Width * grain_image.Height * grain_image.Comp;

    for (int i = 0; i < image.Data.Height; ++i)
    {
        for (int j = 0; j < image.Data.Width; ++j)
        {
            int pixel_index = i * image.Data.Width * image.Data.Comp + j * image.Data.Comp;

            int i0 = pixel_index + 0;
            int i1 = pixel_index + 1;
            int i2 = pixel_index + 2;

            float rgb1[3], rgb0[3] = {
                image.Data.Pixels[i0] / 255.0f,
                image.Data.Pixels[i1] / 255.0f,
                image.Data.Pixels[i2] / 255.0f,
            };

            ApplyLUT(rgb0, rgb1, lut, pow(lut_image.Width, 1.0f / 3.0f));

            if (process_params.CPUPipeline)
            {
                float hsv[3];

                rgb0[0] = SRGB2Linear(rgb0[0]);
                rgb0[1] = SRGB2Linear(rgb0[1]);
                rgb0[2] = SRGB2Linear(rgb0[2]);

                RGB2HSV(rgb0, hsv);

                hsv[0] *= process_params.Hue;
                hsv[1] *= process_params.Saturation;
                hsv[2] *= process_params.Lightness;

                HSV2RGB(hsv, rgb0);

                rgb1[0] = Mix(SRGB2Linear(rgb1[0]), rgb0[0], process_params.LUTStrength);
                rgb1[1] = Mix(SRGB2Linear(rgb1[1]), rgb0[1], process_params.LUTStrength);
                rgb1[2] = Mix(SRGB2Linear(rgb1[2]), rgb0[2], process_params.LUTStrength);

                float cb_bias = (0.5f - process_params.Contrast * 0.5f) + process_params.Brightness;

                rgb1[0] = rgb1[0] * process_params.Contrast + cb_bias;
                rgb1[1] = rgb1[1] * process_params.Contrast + cb_bias;
                rgb1[2] = rgb1[2] * process_params.Contrast + cb_bias;

                int grain_pixel_index = i * grain_image.Width * grain_image.Comp + j * grain_image.Comp;
                float grain[3] = {
                    grain_image.Pixels[(grain_pixel_index + 0) % film_grain_size] / 255.0f,
                    grain_image.Pixels[(grain_pixel_index + 1) % film_grain_size] / 255.0f,
                    grain_image.Pixels[(grain_pixel_index + 2) % film_grain_size] / 255.0f,
                };

                ApplyGrain(rgb1, rgb0, grain);

                rgb0[0] = Mix(rgb0[0], rgb1[0], process_params.GrainStrength);
                rgb0[1] = Mix(rgb0[1], rgb1[1], process_params.GrainStrength);
                rgb0[2] = Mix(rgb0[2], rgb1[2], process_params.GrainStrength);

                float half_pixel_width = 0.5f / image.Data.Width;
                float half_pixel_height = 0.5f / image.Data.Height;

                float texture_coordinates[2] {
                    i / float(image.Data.Height) + half_pixel_height,
                    j / float(image.Data.Width) + half_pixel_width,
                };

                ApplyVignette(rgb0, rgb1, texture_coordinates);

                rgb1[0] = Mix(rgb1[0], rgb0[0], process_params.VignetteStrength);
                rgb1[1] = Mix(rgb1[1], rgb0[1], process_params.VignetteStrength);
                rgb1[2] = Mix(rgb1[2], rgb0[2], process_params.VignetteStrength);

                image.ScratchData[i0] = (uint8_t)(Clamp(Linear2SRGB(rgb1[0]), 0.0f, 1.0f) * 255.0f);
                image.ScratchData[i1] = (uint8_t)(Clamp(Linear2SRGB(rgb1[1]), 0.0f, 1.0f) * 255.0f);
                image.ScratchData[i2] = (uint8_t)(Clamp(Linear2SRGB(rgb1[2]), 0.0f, 1.0f) * 255.0f);
            }
            else
            {
                image.ScratchData[i0] = (uint8_t)(rgb1[0] * 255.0f);
                image.ScratchData[i1] = (uint8_t)(rgb1[1] * 255.0f);
                image.ScratchData[i2] = (uint8_t)(rgb1[2] * 255.0f);
            }

            if (image.Data.Comp == 4)
            {
                int i3 = i * image.Data.Width * image.Data.Comp + j * image.Data.Comp + 3;
                image.ScratchData[i3] = 255;
            }
        }
    }

    FreeImage(lut_image);
    FreeImage(grain_image);

    delete[] lut;
}

}
