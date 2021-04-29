#pragma once

#include "Image.h"
#include "Shader.h"

struct GLFWwindow;

class Window
{
public:
    Window(unsigned int with, unsigned int height);
    ~Window();
    void Initialize();
    void Show();
    void LoadImage(const std::string& path);
    void SaveImage(const std::string& path);
    void LoadProfile(const std::string& path);
    void SaveProfile(const std::string& path);

private:
    GLFWwindow* m_Window;
    Image::HistogramDesc* m_Histogram;
    Image::ImageDesc m_Image;
    Image::ProcessParams m_ProcessParams;
    Shader::Program m_Program;
    unsigned int m_Width;
    unsigned int m_Height;
    int m_FilterLUT;
    int m_LastFilterLUT;
};
