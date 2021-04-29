#pragma once

#include <string>

#include "glad/glad.h"

namespace Image
{

struct ImageData
{
    ImageData();
    unsigned char* Pixels;
    int32_t Width;
    int32_t Height;
    int32_t Comp;
};

struct ImageDesc
{
    ImageDesc();
    ~ImageDesc();
    std::string Path;
    uint8_t* ScratchData;
    GLuint Texture;
    GLuint TextureReference;
    GLuint TextureGrain;
    ImageData Data;
};

struct HistogramDesc
{
    HistogramDesc(unsigned int width, unsigned int height);
    ~HistogramDesc();
    void Allocate();
    void Free();
    uint8_t* Red;
    uint8_t* Green;
    uint8_t* Blue;
    float* HistogramRed;
    float* HistogramGreen;
    float* HistogramBlue;
    float MaxValue;
    uint32_t Width;
    uint32_t Height;
};

enum ProcessFilter
{
    ProcessFilterLUT        = 1 << 0,
    ProcessFilterGrain      = 1 << 1,
    ProcessFilterVignette   = 1 << 2,
    ProcessFilterHSV        = 1 << 3,
    ProcessFilterBrightness = 1 << 4,
    ProcessFilterContrast   = 1 << 5,
    ProcessFilterAll        = ~ProcessFilterLUT,
};

struct ProcessParams
{
    const char* LUTFile;
    int LUTIndex;
    const char* GrainFile;
    int GrainIndex;
    float LUTStrength;
    float GrainStrength;
    float VignetteStrength;
    float Hue;
    float Saturation;
    float Lightness;
    float Brightness;
    float Contrast;
    bool CPUPipeline;
};

bool LoadProfile(const char* path, ProcessParams& process_params);

bool SaveProfile(const char* path, const ProcessParams& process_params);

bool LoadImage(const char* path, ImageData& image);

void FreeImage(const ImageData& image_data);

bool SaveImage(const char* path, const ImageDesc& image);

#ifdef DSIP_GUI

void CaptureHistogram(HistogramDesc& histogram, unsigned int width, unsigned int height);

#endif

void ProcessImage(ImageDesc& image, ProcessParams process_params);

}
