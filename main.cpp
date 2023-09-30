#include <cstring>
#include <cmath>
#include <complex.h>
#include <iostream>
#include <iomanip>
#include <chrono>

#include "util/fft_handler.h"
#include "util/ring_buffer.tcc"
#include "util/math.tcc"
#include "util/rolling_window.tcc"

#include "visualization/bandpass_standing_wave.tcc"

#include <SDL2/SDL.h>
#include "sdl/sdl_audio.tcc"
#define WINDOW_HEIGHT 1000
#define WINDOW_WIDTH 1800

#include "BTrack.h"

#include <string>
#include <iostream>
#include <vector>
#include <algorithm>


// Graphics includes
#include <glad.h>

// GLFW library to create window and to manage I/O
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

// classes developed during lab lectures to manage shaders and to load models
#include "shader.h"
#include <linmath.h>

// Load imgui for the User interface
#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

// dimensions of application's window
GLuint screenWidth = 1200, screenHeight = 1000;

/////////////////////////////////// DEKLARE FUNCTIONS//////////////////////////////////////////////////////////////////////
// callback functions for keyboard events
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mode);
void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void render_glfw_texture();


// initialise Mouse informations
GLfloat deltaTime;
GLfloat lastFrame = 0.0f;
GLfloat aspect_ratio = 1.1f;


void key_callback(GLFWwindow* window, int key, int scancode, int action, int mode)
{
    // if ESC is pressed, we close the application
    if(key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, GL_TRUE);

}

void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    aspect_ratio = width / ((float) height);
    glViewport(0, 0, width, height);
}


typedef std::chrono::high_resolution_clock clk;
double time_diff_us (std::chrono::time_point<clk> const& a, std::chrono::time_point<clk> const& b) {
    const std::chrono::duration<double, std::micro> diff = b - a;
    return diff.count();
}


template <typename SampleT>
int sloth_mainloop (uint16_t device_id, SDL_AudioSpec& spec, BTrack& btrack, size_t num_buffers_delay,
    VisualizationHandler** handlers, size_t num_handlers, double print_interval_ms/*, double* phase_offset, double* phase_offset_2*/) {

    using namespace audio;

    printf("Allocating ring buffer of %.3f kB\n", 2 * spec.samples * spec.channels * num_buffers_delay / 1000.0);
    printf("Audio input delay of %.1f ms\n", num_buffers_delay * spec.samples / (double) spec.freq * 1000.0);
    RingBuffer<SampleT>* ringBuffer = new RingBuffer<SampleT>(spec.channels * spec.samples, num_buffers_delay);
    double* mono = new double[spec.samples];
    math::ExpFilter<double> max_filter(1, 0.90, 0.04, 1);

    auto device_names = get_audio_device_names();
    std::cout << "Starting audio stream on \"" << device_names[device_id] << "\"" << std::endl;
    auto stop_audio_stream = start_audio_stream(ringBuffer, spec, device_id);

    printf("Starting UI\n\n");
    // ui_init();

    auto last_frame = clk::now();
    auto last_print = clk::now();
    double frame_us_nominal = (spec.samples / (double) spec.freq) * 1000000;
    double frame_us_acc = 0;
    double frame_us_transfer_acc = 0;
    size_t frame_counter = 0;

    int samples_since_last_beat = 0;




    // Initialization of OpenGL context using GLFW
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    // we set if the window is resizable
    glfwWindowHint(GLFW_RESIZABLE, GL_TRUE);

    // we create the application's window
    GLFWwindow* window = glfwCreateWindow(screenWidth, screenHeight, "RTGPProject", nullptr, nullptr);
    if (!window)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);

    // GLAD tries to load the context set by GLFW
    if (!gladLoadGLLoader((GLADloadproc) glfwGetProcAddress))
    {
        std::cout << "Failed to initialize OpenGL context" << std::endl;
        return -1;
    }

    // we put in relation the window and the callbacks
    glfwSetKeyCallback(window, key_callback);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);


    // we define the viewport dimensions
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    glViewport(0, 0, width, height);
    aspect_ratio = width / ((double) height);

    //imgui
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    (void) io;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 410");

    //the "clear" color for the frame buffer
    glClearColor(0.05f, 0.05f, 0.05f, 1.0f);

    // we create the Shader Programs used in the application
    Shader mainShader("/home/gelx/Programming/repos/sloth3/graphics/shaders/vertex.vert", "/home/gelx/Programming/repos/sloth3/graphics/shaders/fragment.frag");

    // Rendering loop
    while(!glfwWindowShouldClose(window))
    {
        // SDL_ResizeEvent event = event;
        // double const resize_to[2] = event.type == SDL_VIDEORESIZE ? {event.w, event.h}

        auto now = clk::now();
        frame_counter++;
        frame_us_acc += time_diff_us(last_frame, now);
        if (time_diff_us(last_print, now) / 1000 >= print_interval_ms) {
            double frame_avg_us = frame_us_acc / frame_counter;
            frame_us_acc = 0;
            last_print = now;
            std::cout << "Frame after " << std::setw(10) << frame_avg_us << " us | "
                << std::setw(8) << std::fixed << std::setprecision(2)
                << frame_avg_us / frame_us_nominal * 100 << "% utilization\t"
                << frame_us_transfer_acc / frame_counter << " avg transfer time [us]\n";
            frame_counter = 0;
        }

        SampleT* const buf = ringBuffer->dequeue_dirty();
        last_frame = clk::now();

        for (size_t i = 0; i < spec.samples; i++) {
            mono[i] = buf[2*i] / ((double) std::pow(2, 16));//(buf[2*i] + buf[2*i + 1]) / ((double) std::pow(2, 17));
        }

        memset(buf, spec.silence, spec.channels * sizeof(SampleT) * spec.samples);
        ringBuffer->enqueue_clean(buf);

        // Maximum filter
        double maxval = math::max_value(mono, spec.samples);
        maxval = *max_filter.update(&maxval);
        maxval = maxval > 2 ? 2 : (maxval < 0.01 ? 0.02 : maxval);
        for (size_t i = 0; i < spec.samples; i++) {
            mono[i] /= 2.5 * maxval;
        }

        // std::cout << "Running btrack on new buffer" << std::endl;
        // btrack.getBeatTimeInSeconds()
        // std::cout << "beatCounter: " << btrack.beatCounter << std::endl;
        // samples_since_last_beat += spec.samples;

        btrack.processAudioFrame(mono);
        bool is_new_beat = btrack.beatDueInCurrentFrame();
        double tempo_estimate = btrack.getCurrentTempoEstimate();

        VisualizationBuffer const data {
            .audio_buffer = mono,
            .tempo_estimate = tempo_estimate,
            .is_new_beat = is_new_beat
        };

        if (is_new_beat) {
            // *phase_offset += 200;
            // *phase_offset_2 += 200;
            // samples_since_last_beat = 0;
            std::cout << "Tempo: " << btrack.getCurrentTempoEstimate() << " BPM" << std::endl;
            // double bpm = btrack.getCurrentTempoEstimate();
            // double period_s = 60 / bpm;
            // double samples = spec.freq * period_s;
            // std::cout << "period_s: " << period_s << "\t samples: " << samples << std::endl;

            // for (size_t i = 0; i < num_handlers; i++) {

            /*       CURSED      */

            //     ((BandpassStandingWave*)handlers[i])->params->crop_length_samples = ;
            // }
        }


        for (size_t i = 0; i < num_handlers; i++) {
            handlers[i]->process_ring_buffer(data);
        }


        GLint line_lengths[10];
        GLfloat** results = new GLfloat* [num_handlers];
        for (size_t i = 0; i < num_handlers; i++) {
            int result_size = handlers[i]->get_result_size();
            line_lengths[i] = result_size;
            results[i] = new GLfloat[result_size];
            handlers[i]->await_result(((float*)results[i]));
        }

        auto before_us = clk::now();


        // we determine the time passed from the beginning
        // and we calculate time difference between current frame rendering and the previous one
        GLfloat currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        // we "clear" the frame and z buffer
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        mainShader.Use();


        int const NUM_LINES = 10;
        glUniform1f(glGetUniformLocation(mainShader.Program, "aspect_ratio"), aspect_ratio);
        glUniform1f(glGetUniformLocation(mainShader.Program, "timer"), currentFrame);

        glUniform1iv(glGetUniformLocation(mainShader.Program, "line_lengths"), NUM_LINES, line_lengths);


        for (int i = 0; i < NUM_LINES; i++) {
            std::string var_name = "line_data_";
            var_name.append(std::to_string(i));
            glUniform1fv(glGetUniformLocation(mainShader.Program, var_name.c_str()), line_lengths[i], results[i]);
        }

        render_glfw_texture();
        frame_us_transfer_acc += time_diff_us(before_us, clk::now());

        // Check is an I/O event is happening
        glfwPollEvents();

        /////////////////////////////////// IMGUI INTERFACE /////////////////////////////////////////////////////////////////////////////
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        // ImGUI window creation
        ImGui::Begin("Performance");

        // Ends of imgui
        ImGui::End();
        // Renders the ImGUI elements
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

        // Swapping back and front buffers
        glfwSwapBuffers(window);
        for (int i = 0; i < num_handlers; i++) {
            delete[] results[i];
        }
        delete[] results;

    }

    // when I exit from the graphics loop, it is because the application is closing
    // we delete the Shader Programs
    mainShader.Delete();

    //Delete the IMGui context
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();


    // we close and delete the created context
    glfwTerminate();
    return 0;

    printf("\n\nStopping audio stream and deconstructing.\n");

    // ui_shutdown();
    // stop_audio_stream();

    delete ringBuffer;
    delete[] mono;
}

int main (int argc, char** argv) {
    std::cout << "Starting sloth2 realtime audio visualizer..." << std::endl;

    using namespace audio;
    sdl_init();

    uint16_t device_id = 0;
    if (argc > 1) {
	   device_id = atoi(argv[1]);
    } else {
        auto device_names = get_audio_device_names();
        std::cout << "\nNo audio device specified. Please choose one!" << std::endl;
        std::cout << "Usage: \"./sloth2 <device_id>\"\n" << std::endl;
        std::cout << "Available devices:" << std::endl;
        for (size_t i = 0; i < device_names.size(); i++) {
            std::cout << "\t" << i << ": " << device_names[i] << std::endl;
        }

        return 1;
    }

    std::cout << std::endl << std::endl;

    /* -------------------- CONFIGURATION --------------------------------- */

    SDL_AudioSpec spec;
    SDL_zero(spec);

    typedef int16_t SampleT;
    spec.format = AUDIO_S16SYS;
    spec.freq = 48000;
    spec.channels = 2;

    const static double update_interval_ms = 1000 / 60;
    const static double window_length_ms = 300;
    const static double print_interval_ms = 2000;
    const static int num_buffers_delay = 1;

    size_t window_length_samples = window_length_ms / 1000 * spec.freq;
    spec.samples = (size_t) update_interval_ms / 1000.0 * spec.freq;

    BPSW_Spec params {
        .win_length_samples = window_length_samples,
        .update_length_samples = spec.samples,
        .win_window_fn = true,
        .adaptive_crop = true,
        .fft_dispersion = 0, // -0.1
        .fft_phase = BPSW_Phase::Standing,
        .fft_phase_const = 1,
        .crop_length_samples = window_length_samples,
        .crop_offset = 0,
        .c_center_x = WINDOW_WIDTH / 2,
        .c_center_y = WINDOW_HEIGHT / 2,
        .c_rad_base = WINDOW_HEIGHT / 2 - 80,
        .c_rad_extr = 300,
    };
    size_t c_length = params.win_length_samples / 2 + 1;
    double* freq_weighing = new double[c_length];
    for (size_t i = 0; i < c_length; i++) {
        freq_weighing[i] = i < 10 ? 1.5 :
                           i < 40 ? 1 : 0.1;
    }
    params.fft_freq_weighing = freq_weighing;


    BPSW_Spec params_inner {
        .win_length_samples = window_length_samples,
        .update_length_samples = spec.samples,
        .win_window_fn = true,
        .adaptive_crop = false,
        .fft_dispersion = -0.1, // -0.1
        .fft_phase = BPSW_Phase::Unchanged,
        .fft_phase_const = 0.8,
        .crop_length_samples = window_length_samples - 800,
        .crop_offset = 400,
        .c_center_x = WINDOW_WIDTH / 2,
        .c_center_y = WINDOW_HEIGHT / 2,
        .c_rad_base = WINDOW_HEIGHT / 2 - 400,
        .c_rad_extr = 400,
    };
    size_t c_length_i = params_inner.win_length_samples / 2 + 1;
    double* freq_weighing_inner = new double[c_length_i];
    for (size_t i = 0; i < c_length_i; i++) {
        freq_weighing_inner[i] = i < 200 ? 0 : ( i > (c_length_i - 1200) ? 0 : 1);
    }
    params_inner.fft_freq_weighing = freq_weighing_inner;


    // BPSW_Spec params2 {
    //     .win_length_samples = window_length_samples / 2,
    //     .win_window_fn = true,
    //     .fft_dispersion = 0,
    //     .fft_phase = BPSW_Phase::Standing,
    //     .fft_phase_const = 0,
    //     .crop_length_samples = window_length_samples / 2,
    //     .crop_offset = 0,
    //     .c_center_x = WINDOW_WIDTH / 2,
    //     .c_center_y = WINDOW_HEIGHT / 2,
    //     .c_rad_base = WINDOW_HEIGHT / 2 - 50,
    //     .c_rad_extr = 300,
    // };
    // size_t c_length_2 = params2.win_length_samples / 2 + 1;
    // double* freq_weighing_2 = new double[c_length_2];
    // for (size_t i = 0; i < c_length_2; i++) {
    //     freq_weighing_2[i] = i > 110 ? 3 :
    //                          i > 50 ? 1.5 : 0;
    // }
    // params2.fft_freq_weighing = freq_weighing_2;

    /* -------------------- CONFIGURATION END ----------------------------- */

    printf("Instantiating visualizations\n");
    BandpassStandingWave bpsw {spec, &params};
    BandpassStandingWave bpsw_inner {spec, &params_inner};
    printf("Done\n");
    // BandpassStandingWave bpsw_i {spec, params_i};
    // BandpassStandingWave bpsw2 {spec, params2};

    constexpr size_t num_handlers = 2;
    printf("Instantiating visualization handler\n");
    VisualizationHandler* handlers[num_handlers] = {&bpsw_inner, &bpsw/*, &bpsw2*/};
    printf("Done\n");

    std::cout << "Initializing BTrack with " << spec.samples << " samples" << std::endl;
    BTrack btrack(spec.freq, spec.samples / 2, spec.samples);

    return sloth_mainloop<SampleT>(device_id, spec, btrack, num_buffers_delay, handlers, num_handlers, print_interval_ms);
}


void render_glfw_texture()
{

    float floatArray[] = {
        -1.0,  1.0, 0.0,
         1.0,  1.0, 0.0,
        -1.0, -1.0, 0.0,
         1.0, -1.0, 0.0
    };

    int indices[] = {
        0, 1, 2,
        1, 2, 3
    };

    GLuint VBO, VAO, EBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO); 
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(floatArray), floatArray, GL_STATIC_DRAW); 

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);    

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), (GLvoid*)0); 

    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

    glBindVertexArray(0);

}