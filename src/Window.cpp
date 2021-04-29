#include "Window.h"

#include "GLFW/glfw3.h"

#include "imgui.cpp"
#include "imgui_draw.cpp"
#include "imgui_impl_glfw_gl3.cpp"

#include "LUTs.h"
#include "Util.h"
#include "FilmGrain.h"

Window::Window(unsigned int width, unsigned int height)
    : m_Histogram(nullptr)
    , m_Width(width)
    , m_Height(height)
    , m_FilterLUT(-1)
    , m_LastFilterLUT(0)
{
}

Window::~Window()
{
    delete m_Histogram;
}

void Window::LoadImage(const std::string& path)
{
    Image::FreeImage(m_Image.Data);

    if (!Image::LoadImage(path.c_str(), m_Image.Data)) return;

    m_Image.Path = path;

    m_Width = m_Image.Data.Width;
    m_Height = m_Image.Data.Height;

    float width_ratio = 1024.0f / m_Width;
    float height_ratio = 768.0f / m_Height;
    float ratio = std::min<float>(width_ratio, height_ratio);
    if (ratio < 1.0f)
    {
        m_Width *= ratio;
        m_Height *= ratio;
    }

    glfwSetWindowSize(m_Window, m_Width, m_Height);

    GLuint format = m_Image.Data.Comp == 4 ? GL_RGBA : GL_RGB;

    glGenTextures(1, &m_Image.TextureReference);
    glBindTexture(GL_TEXTURE_2D, m_Image.TextureReference);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, m_Image.Data.Width, m_Image.Data.Height, 0, format, GL_UNSIGNED_BYTE, m_Image.Data.Pixels);

    glGenTextures(1, &m_Image.Texture);
    glBindTexture(GL_TEXTURE_2D, m_Image.Texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    Image::ProcessImage(m_Image, m_ProcessParams);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, m_Image.Data.Width, m_Image.Data.Height, 0, format, GL_UNSIGNED_BYTE, m_Image.ScratchData);

    Image::ImageData grain_image;

    Image::LoadImage(m_ProcessParams.GrainFile, grain_image);

    glGenTextures(1, &m_Image.TextureGrain);
    glBindTexture(GL_TEXTURE_2D, m_Image.TextureGrain);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, grain_image.Width, grain_image.Height, 0, GL_RGB, GL_UNSIGNED_BYTE, grain_image.Pixels);

    Image::FreeImage(grain_image);

    delete m_Histogram;

    m_Histogram = new Image::HistogramDesc(m_Width, m_Height);
}

void Window::SaveImage(const std::string& path)
{
    Image::ImageDesc image = m_Image;
    Image::ProcessParams process_params = m_ProcessParams;
    process_params.CPUPipeline = true;

    Image::ProcessImage(image, process_params);
    Image::SaveImage(path.c_str(), image);
}

void Window::LoadProfile(const std::string& path)
{
    Image::LoadProfile(path.c_str(), m_ProcessParams);

    m_LastFilterLUT = m_ProcessParams.LUTIndex;
}

void Window::SaveProfile(const std::string& path)
{
    m_ProcessParams.LUTIndex = m_FilterLUT;

    Image::SaveProfile(path.c_str(), m_ProcessParams);
}

void Window::Initialize()
{
    glfwInit();

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SAMPLES, 4);
    glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);

    m_Window = glfwCreateWindow(m_Width, m_Height, "", NULL, NULL);

    glfwSetWindowUserPointer(m_Window, this);
    glfwMakeContextCurrent(m_Window);

    glfwSetDropCallback(m_Window, [](GLFWwindow* window, int count, const char** paths)
    {
        Window* _this = static_cast<Window*>(glfwGetWindowUserPointer(window));
        _this->LoadImage(paths[0]);
    });

    gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
    glfwSwapInterval(1);

    ImGui_ImplGlfwGL3_Init(m_Window, true);

    glfwSetKeyCallback(m_Window, [](GLFWwindow* window, int key, int scancode, int action, int mods)
    {
        Window* _this = static_cast<Window*>(glfwGetWindowUserPointer(window));
        if (action == GLFW_PRESS)
        {
            switch (key) {
               case GLFW_KEY_DOWN:
               case GLFW_KEY_RIGHT:
                   _this->m_LastFilterLUT++;
                   break;
               case GLFW_KEY_UP:
               case GLFW_KEY_LEFT:
                   _this->m_LastFilterLUT--;
                    break;
               default:
                   break;
            }
            if (_this->m_LastFilterLUT < 0)
                _this->m_LastFilterLUT = 0;
            _this->m_LastFilterLUT = _this->m_LastFilterLUT % IM_ARRAYSIZE(LUTs);
        }
    });

    ImGuiStyle& style = ImGui::GetStyle();

    style.WindowMinSize     = ImVec2(300, 5000);
    style.FramePadding      = ImVec2(6, 6);
    style.ItemSpacing       = ImVec2(6, 6);
    style.ItemInnerSpacing  = ImVec2(6, 6);
    style.Alpha             = 1.0f;
    style.WindowRounding    = 0.0f;
    style.FrameRounding     = 0.0f;
    style.IndentSpacing     = 6.0f;
    style.ItemInnerSpacing  = ImVec2(6, 6);
    style.ColumnsMinSpacing = 50.0f;
    style.GrabMinSize       = 14.0f;
    style.GrabRounding      = 0.0f;
    style.ScrollbarSize     = 12.0f;
    style.ScrollbarRounding = 0.0f;

    style.Colors[ImGuiCol_Text]                  = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
    style.Colors[ImGuiCol_TextDisabled]          = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
    style.Colors[ImGuiCol_WindowBg]              = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
    style.Colors[ImGuiCol_ChildWindowBg]         = ImVec4(0.20f, 0.20f, 0.20f, 0.58f);
    style.Colors[ImGuiCol_Border]                = ImVec4(0.31f, 0.31f, 0.31f, 0.00f);
    style.Colors[ImGuiCol_BorderShadow]          = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    style.Colors[ImGuiCol_FrameBg]               = ImVec4(0.20f, 0.20f, 0.20f, 0.60f);
    style.Colors[ImGuiCol_FrameBgHovered]        = ImVec4(0.92f, 0.18f, 0.29f, 0.78f);
    style.Colors[ImGuiCol_FrameBgActive]         = ImVec4(0.92f, 0.18f, 0.29f, 1.00f);
    style.Colors[ImGuiCol_TitleBg]               = ImVec4(0.20f, 0.22f, 0.27f, 1.00f);
    style.Colors[ImGuiCol_TitleBgCollapsed]      = ImVec4(0.20f, 0.22f, 0.27f, 0.75f);
    style.Colors[ImGuiCol_TitleBgActive]         = ImVec4(0.92f, 0.18f, 0.29f, 1.00f);
    style.Colors[ImGuiCol_MenuBarBg]             = ImVec4(0.20f, 0.22f, 0.27f, 0.47f);
    style.Colors[ImGuiCol_ScrollbarBg]           = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
    style.Colors[ImGuiCol_ScrollbarGrab]         = ImVec4(0.47f, 0.47f, 0.47f, 0.21f);
    style.Colors[ImGuiCol_ScrollbarGrabHovered]  = ImVec4(0.92f, 0.18f, 0.29f, 0.78f);
    style.Colors[ImGuiCol_ScrollbarGrabActive]   = ImVec4(0.92f, 0.18f, 0.29f, 1.00f);
    style.Colors[ImGuiCol_ComboBg]               = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
    style.Colors[ImGuiCol_CheckMark]             = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
    style.Colors[ImGuiCol_SliderGrab]            = ImVec4(0.47f, 0.47f, 0.47f, 0.14f);
    style.Colors[ImGuiCol_SliderGrabActive]      = ImVec4(0.92f, 0.18f, 0.29f, 1.00f);
    style.Colors[ImGuiCol_Button]                = ImVec4(0.47f, 0.47f, 0.47f, 0.14f);
    style.Colors[ImGuiCol_ButtonHovered]         = ImVec4(0.92f, 0.18f, 0.29f, 0.86f);
    style.Colors[ImGuiCol_ButtonActive]          = ImVec4(0.92f, 0.18f, 0.29f, 1.00f);
    style.Colors[ImGuiCol_Header]                = ImVec4(0.92f, 0.18f, 0.29f, 0.76f);
    style.Colors[ImGuiCol_HeaderHovered]         = ImVec4(0.92f, 0.18f, 0.29f, 0.86f);
    style.Colors[ImGuiCol_HeaderActive]          = ImVec4(0.92f, 0.18f, 0.29f, 1.00f);
    style.Colors[ImGuiCol_Column]                = ImVec4(0.47f, 0.77f, 0.83f, 0.32f);
    style.Colors[ImGuiCol_ColumnHovered]         = ImVec4(0.92f, 0.18f, 0.29f, 0.78f);
    style.Colors[ImGuiCol_ColumnActive]          = ImVec4(0.92f, 0.18f, 0.29f, 1.00f);
    style.Colors[ImGuiCol_ResizeGrip]            = ImVec4(0.47f, 0.77f, 0.83f, 0.04f);
    style.Colors[ImGuiCol_ResizeGripHovered]     = ImVec4(0.92f, 0.18f, 0.29f, 0.78f);
    style.Colors[ImGuiCol_ResizeGripActive]      = ImVec4(0.92f, 0.18f, 0.29f, 1.00f);
    style.Colors[ImGuiCol_CloseButton]           = ImVec4(0.86f, 0.93f, 0.89f, 0.16f);
    style.Colors[ImGuiCol_CloseButtonHovered]    = ImVec4(0.86f, 0.93f, 0.89f, 0.39f);
    style.Colors[ImGuiCol_CloseButtonActive]     = ImVec4(0.86f, 0.93f, 0.89f, 1.00f);
    style.Colors[ImGuiCol_PlotLines]             = ImVec4(0.86f, 0.93f, 0.89f, 0.63f);
    style.Colors[ImGuiCol_PlotLinesHovered]      = ImVec4(0.92f, 0.18f, 0.29f, 1.00f);
    style.Colors[ImGuiCol_PlotHistogram]         = ImVec4(0.86f, 0.86f, 0.86f, 0.63f);
    style.Colors[ImGuiCol_PlotHistogramHovered]  = ImVec4(0.92f, 0.18f, 0.29f, 1.00f);
    style.Colors[ImGuiCol_TextSelectedBg]        = ImVec4(0.92f, 0.18f, 0.29f, 0.43f);
    style.Colors[ImGuiCol_ModalWindowDarkening]  = ImVec4(0.20f, 0.22f, 0.27f, 0.73f);
    style.Colors[ImGuiCol_PopupBg]               = ImVec4(0.20f, 0.20f, 0.20f, 0.50f);

    m_Program = Shader::CompileDefaultProgram();

    int width, height;
    glfwGetFramebufferSize(m_Window, &width, &height);
    glViewport(0, 0, width, height);

    Util::SetupPlatformMenu(this);

    srand(time(nullptr));

    float rand_0_1 = (float)rand() / RAND_MAX;
    int rand_index = rand_0_1 * (IM_ARRAYSIZE(FilmGrain) - 1);

    m_ProcessParams.LUTFile = LUTs[0];
    m_ProcessParams.LUTIndex = 0;
    m_ProcessParams.GrainFile = FilmGrain[rand_index];
    m_ProcessParams.GrainIndex = rand_index;
    m_ProcessParams.LUTStrength = 0.0f;
    m_ProcessParams.GrainStrength = 0.0f;
    m_ProcessParams.VignetteStrength = 0.0f;
    m_ProcessParams.Hue = 1.0f;
    m_ProcessParams.Saturation = 1.0f;
    m_ProcessParams.Lightness = 1.0f;
    m_ProcessParams.Brightness = 0.0f;
    m_ProcessParams.Contrast = 1.0f;

    // On the CPU side, only apply the LUT
    m_ProcessParams.CPUPipeline = false;
}

void Window::Show()
{
    while (!glfwWindowShouldClose(m_Window))
    {
        glfwPollEvents();

        ImGui_ImplGlfwGL3_NewFrame();

        ImGuiWindowFlags options = ImGuiWindowFlags_NoTitleBar
            | ImGuiWindowFlags_NoResize
            | ImGuiWindowFlags_NoMove;

        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::Begin("Fixed Overlay", nullptr, ImVec2(0, 0), 0.3f, options);
        ImGui::Combo("LUT", &m_LastFilterLUT, LUTs, IM_ARRAYSIZE(LUTs));
        ImGui::SliderFloat("LUT", &m_ProcessParams.LUTStrength, 0.0, 1.0);
        ImGui::SliderFloat("Grain", &m_ProcessParams.GrainStrength, 0.0, 1.0);
        ImGui::SliderFloat("Vignette", &m_ProcessParams.VignetteStrength, 0.0, 1.0);
        ImGui::SliderFloat("Hue", &m_ProcessParams.Hue, 0.0, 1.0);
        ImGui::SliderFloat("Saturation", &m_ProcessParams.Saturation, 0.0, 1.0);
        ImGui::SliderFloat("Lightness", &m_ProcessParams.Lightness, 0.0, 1.0);
        ImGui::SliderFloat("Brightness", &m_ProcessParams.Brightness, -0.2, 0.2);
        ImGui::SliderFloat("Contrast", &m_ProcessParams.Contrast, 0.8, 1.2);

        if (m_FilterLUT != m_LastFilterLUT)
        {
            m_FilterLUT = m_LastFilterLUT;
            m_ProcessParams.LUTFile = LUTs[m_FilterLUT];
            ProcessImage(m_Image, m_ProcessParams);
            GLuint format = m_Image.Data.Comp == 4 ? GL_RGBA : GL_RGB;
            glBindTexture(GL_TEXTURE_2D, m_Image.Texture);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, m_Image.Data.Width, m_Image.Data.Height, 0, format, GL_UNSIGNED_BYTE, m_Image.ScratchData);
        }

        glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);

        glUseProgram(m_Program.ID);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_Image.Texture);

        glUniform1i(m_Program.Texture, 0);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, m_Image.TextureReference);

        glUniform1i(m_Program.TextureReference, 1);
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, m_Image.TextureGrain);

        glUniform1i(m_Program.TextureGrain, 2);
        glUniform2f(m_Program.TextureResolution, (float)m_Image.Data.Width, (float)m_Image.Data.Height);
        glUniform1f(m_Program.LUTStrength, m_ProcessParams.LUTStrength);
        glUniform1f(m_Program.GrainStrength, m_ProcessParams.GrainStrength);
        glUniform1f(m_Program.VignetteStrength, m_ProcessParams.VignetteStrength);
        glUniform1f(m_Program.Hue, m_ProcessParams.Hue);
        glUniform1f(m_Program.Saturation, m_ProcessParams.Saturation);
        glUniform1f(m_Program.Lightness, m_ProcessParams.Lightness);
        glUniform1f(m_Program.Brightness, m_ProcessParams.Brightness);
        glUniform1f(m_Program.Contrast, m_ProcessParams.Contrast);

        glBindVertexArray(m_Program.VAO);

        glDrawArrays(GL_TRIANGLES, 0, m_Program.VertexCount);

        if (m_Histogram)
        {
            //Image::CaptureHistogram(*m_Histogram, m_Width, m_Height);
            //ImGui::PlotHistogram("Red", m_Histogram->HistogramRed, 255, 0, NULL, 0.0f, m_Histogram->MaxValue, ImVec2(0,120));
            //ImGui::PlotHistogram("Green", m_Histogram->HistogramGreen, 255, 0, NULL, 0.0f, m_Histogram->MaxValue, ImVec2(0,120));
            //ImGui::PlotHistogram("Blue", m_Histogram->HistogramBlue, 255, 0, NULL, 0.0f, m_Histogram->MaxValue, ImVec2(0,120));
        }

        ImGui::End();
        ImGui::Render();

        glfwSwapBuffers(m_Window);
    }
}
