#ifdef DSIP_GUI
#include "Window.h"
#endif
#include "FilmGrain.h"
#include "Image.h"

extern "C"
{
    #include "flag.h"
}

#define ARRAYSIZE(_ARR) ((int)(sizeof(_ARR) / sizeof(*_ARR)))

struct CLIOptions
{
    const char* ImageInput;
    const char* ImageProfile;
    const char* ImageOutput;
};

bool ValidateOptions(CLIOptions options)
{
    return options.ImageInput && options.ImageOutput && options.ImageProfile;
}

int Process(CLIOptions options)
{
    Image::ImageDesc image;

    bool res = Image::LoadImage(options.ImageInput, image.Data);

    if (!res) return EXIT_FAILURE;

    Image::ProcessParams process_params;

    res = Image::LoadProfile(options.ImageProfile, process_params);

    if (!res) return EXIT_FAILURE;

    float rand_0_1 = (float)rand() / RAND_MAX;
    int rand_index = rand_0_1 * (ARRAYSIZE(FilmGrain) - 1);

    process_params.CPUPipeline = true;
    process_params.GrainFile = FilmGrain[rand_index];

    Image::ProcessImage(image, process_params);
    
    res = Image::SaveImage(options.ImageOutput, image);

    Image::FreeImage(image.Data);

    return res;
}

int main(int argc, const char** argv)
{
    CLIOptions options;

    flag_usage("[options]");

    flag_string(&options.ImageInput, "input", "Image path to process");
    flag_string(&options.ImageProfile, "profile", "Image profile params");
    flag_string(&options.ImageOutput, "output", "Image path result");

    flag_parse(argc, argv, "v" "0.1.0", 0);

    if (ValidateOptions(options))
    {
        return Process(options);
    }

#ifdef DSIP_GUI
    Window window(800, 800);
    window.Initialize();
    window.Show();
#endif

    return EXIT_SUCCESS;
}
