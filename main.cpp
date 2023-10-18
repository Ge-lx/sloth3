#include <cstring>
#include <cmath>
#include <complex.h>
#include <iostream>
#include <iomanip>
#include <chrono>
#include <string>
#include <vector>
#include <algorithm>

// Graphics includes
#include <glad.h>

// GLFW library to create window and to manage I/O
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

// Load imgui for the User interface
#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

// Linear algebra subroutines
#include "linmath.h"

// Beat tracking algorithm
#include "BTrack.h"

#include "util/fft_handler.h"
#include "util/ring_buffer.tcc"
#include "util/math.tcc"
#include "util/rolling_window.tcc"
#include "util/sdl_audio.tcc"
#include "visualization/bandpass_standing_wave.tcc"
#include "graphics/shader.h"
#include "graphics/shader_locations.h"


// dimensions of application's window
GLuint screenWidth = 1200, screenHeight = 1000;

// callback functions for glfw
void glfw_key_callback(GLFWwindow* window, int key, int scancode, int action, int mode);
void glfw_framebuffer_size_callback(GLFWwindow* window, int width, int height);
void glfw_render_texture();


// initialise Mouse informations
GLfloat pattern_scale = 4271.0f;
GLfloat movement_scale = 10208.0f;
GLfloat time_scale = 26000.0f;
GLfloat delta_time_4_s = 0.0f;
GLfloat delta_time_1_s = 0.0f;
GLfloat period_s = 10.0f;
GLfloat lastFrame = 0.0f;
GLfloat aspect_ratio = 1.1f;
GLfloat color_bg[4] = {0.05, 0.05, 0.05, 1.0};
GLuint beat_counter = 0;


struct __attribute__ ((packed)) LineParams {
    GLfloat color_inner_0;
    GLfloat color_inner_1;
    GLfloat color_inner_2;
    GLfloat radius_base;
    GLfloat radius_scale;
    GLfloat data_end_idx;
    GLuint buffer_length;
    GLuint num_aux_lines;
};

LineParams build_line_params (BPSW_Spec const& spec, size_t buffer_length, size_t data_end_idx) {
    return LineParams{
        .color_inner_0 = spec.color_inner[0],
        .color_inner_1 = spec.color_inner[1],
        .color_inner_2 = spec.color_inner[2],
        .radius_base = (float) spec.c_rad_base,
        .radius_scale = (float) spec.c_rad_extr,
        .data_end_idx = (GLfloat) data_end_idx,
        .buffer_length = (GLuint) buffer_length,
        .num_aux_lines = 0
    };
}

void glfw_key_callback(GLFWwindow* window, int key, int /*scancode*/, int action, int /*mode*/)
{
    // if ESC is pressed, we close the application
    if(key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, GL_TRUE);

}

void glfw_framebuffer_size_callback(GLFWwindow* /*window*/, int width, int height)
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
    VisualizationHandler** handlers, size_t const num_handlers, double print_interval_ms, unsigned int const target_fps) {

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
    size_t frame_counter = 0;

    // Initialization of OpenGL context using GLFW
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    // we set if the window is resizable
    glfwWindowHint(GLFW_RESIZABLE, GL_TRUE);

    // we create the application's window
    GLFWwindow* window = glfwCreateWindow(screenWidth, screenHeight, "Sloth3", nullptr, nullptr);
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

    // we define the viewport dimensions
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    glfw_framebuffer_size_callback(window, width, height);

    // we put in relation the window and the callbacks
    glfwSetKeyCallback(window, glfw_key_callback);
    glfwSetFramebufferSizeCallback(window, glfw_framebuffer_size_callback);
    glfwSwapInterval(0);


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
    Shader mainShader(SHADER_VERTEX, SHADER_FRAGMENT);

    GLuint ssbo_params;
    glGenBuffers(1, &ssbo_params);
    glBindBuffer(GL_TEXTURE_BUFFER, ssbo_params);
    glBufferData(GL_TEXTURE_BUFFER, 10 * sizeof(LineParams), NULL, GL_STREAM_DRAW);

    GLuint ssbo_data;
    glGenBuffers(1, &ssbo_data);
    glBindBuffer(GL_TEXTURE_BUFFER, ssbo_data);
    glBufferData(GL_TEXTURE_BUFFER, 600000 * sizeof(GLfloat), NULL, GL_STREAM_DRAW);

    GLuint ssbo_aux_data;
    glGenBuffers(1, &ssbo_aux_data);
    glBindBuffer(GL_TEXTURE_BUFFER, ssbo_aux_data);
    glBufferData(GL_TEXTURE_BUFFER, 60000000 * sizeof(GLfloat), NULL, GL_STREAM_DRAW);

    GLuint tex_buf_data;
    glGenTextures(1, &tex_buf_data);

    GLuint tex_buf_params;
    glGenTextures(1, &tex_buf_params);

    GLuint tex_buf_aux_data;
    glGenTextures(1, &tex_buf_aux_data);
    // glUniform1i(glGetUniformLocation(mainShader.Program, "params"), 0);
    // glUniform1i(glGetUniformLocation(mainShader.Program, "data_samples"), 1);
    // glUniform1i(glGetUniformLocation(mainShader.Program, "data_samples_aux"), 2);

    glfwMakeContextCurrent(window);

    // Rendering loop
    while(!glfwWindowShouldClose(window))
    {
        try {
            SampleT* const buf = ringBuffer->dequeue_dirty();
            last_frame = clk::now();

            for (size_t i = 0; i < spec.samples; i++) {
                mono[i] = buf[2*i] / ((double) std::pow(2, 16));//(buf[2*i] + buf[2*i + 1]) / ((double) std::pow(2, 17));
            }

            memset(buf, spec.silence, spec.channels * sizeof(SampleT) * spec.samples);
            ringBuffer->enqueue_clean(buf);
        } catch (const timeout_exception& e) {
            std::cout << "RingBuffer is draining!" << std::endl;
            break;
        }

        // Maximum filter
        double maxval = math::max_value(mono, spec.samples);
        maxval = *max_filter.update(&maxval);
        maxval = maxval > 2 ? 2 : (maxval < 0.01 ? 0.02 : maxval);
        for (size_t i = 0; i < spec.samples; i++) {
            mono[i] /= 2.5 * maxval;
        }

        btrack.processAudioFrame(mono);
        bool is_new_beat = btrack.beatDueInCurrentFrame();
        double tempo_estimate = btrack.getCurrentTempoEstimate();

        VisualizationBuffer const data {
            .audio_buffer = mono,
            .tempo_estimate = tempo_estimate,
            .is_new_beat = is_new_beat
        };

        for (size_t i = 0; i < num_handlers; i++) {
            handlers[i]->process_ring_buffer(data);
        }


        LineParams params[num_handlers];
        for (size_t i = 0; i < num_handlers; i++) {
            handlers[i]->await_buffer_processed(false); // Keep lock from here

            int result_size = handlers[i]->get_result_size();
            BPSW_Spec vis_params = ((BandpassStandingWave*)handlers[i])->params;
            size_t data_end_idx = (i == 0 ? result_size : (params[i-1].data_end_idx + result_size));
            params[i] = build_line_params(vis_params, result_size, data_end_idx);

            handlers[i]->unlock_mutex(); // Unlock
        }

        // Shared result buffer for all handler results
        size_t const total_length = params[num_handlers-1].data_end_idx;
        GLfloat* results_concat = new GLfloat[total_length];
        for (size_t i = 0; i < num_handlers; i++) {
            size_t offset = i == 0 ? 0 : params[i-1].data_end_idx;
            handlers[i]->await_result(((float*)(results_concat + offset)));

            if (is_new_beat) {
                auto& queue = ((BandpassStandingWave*) handlers[i])->data_lookback_beats;
                queue.push_back(
                    std::vector<float>(
                        ((float*) results_concat + offset),
                        ((float*) results_concat + ((size_t) params[i].data_end_idx)))
                );

                if (queue.size() > 4 && queue.size() > 0) {
                    queue.pop_front();
                }
            }
        }


        size_t aux_buffer_total_length = 0;
        for (size_t i = 0; i < num_handlers; i++) {
            BandpassStandingWave* bpsw = ((BandpassStandingWave*) handlers[i]);
            params[i].num_aux_lines = bpsw->data_lookback_beats.size();
            aux_buffer_total_length += params[i].num_aux_lines * params[i].buffer_length;
        }

        // Prepare data buffer for shader buffer object
        GLfloat* aux_buffers_concat = new GLfloat[aux_buffer_total_length];
        size_t total_offset = 0;
        for (size_t i = 0; i < num_handlers; i++) {
            auto& queue = ((BandpassStandingWave*) handlers[i])->data_lookback_beats;
            for (size_t j = 0; j < params[i].num_aux_lines; j++) {
                size_t max_copy_len = std::min(((size_t)queue[j].size()), ((size_t)params[i].buffer_length));
                memcpy((aux_buffers_concat + total_offset), queue[j].data(), max_copy_len * sizeof(GLfloat));
                total_offset += params[i].buffer_length;
            }
        }

        // we determine the time passed from the beginning
        // and we calculate time difference between current frame rendering and the previous one 
        GLfloat shader_timer = glfwGetTime();
        if (is_new_beat) {
            beat_counter = (beat_counter + 1) % 4;
            delta_time_1_s = 0;
            if (beat_counter == 0) {
                delta_time_4_s = 0;
            }
        }
        delta_time_4_s += shader_timer - lastFrame;
        delta_time_1_s += shader_timer - lastFrame;
        lastFrame = shader_timer;
        period_s = 60.0 / tempo_estimate;

        // we "clear" the frame and z buffer
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        mainShader.Use();

        // int const num_handlers = 10;
        glUniform1f(glGetUniformLocation(mainShader.Program, "aspect_ratio"), aspect_ratio);
        glUniform1f(glGetUniformLocation(mainShader.Program, "pattern_scale"), pattern_scale);
        glUniform1f(glGetUniformLocation(mainShader.Program, "movement_scale"), movement_scale);
        glUniform1f(glGetUniformLocation(mainShader.Program, "time_scale"), time_scale);
        glUniform1f(glGetUniformLocation(mainShader.Program, "delta_time_4_s"), delta_time_4_s);
        glUniform1f(glGetUniformLocation(mainShader.Program, "delta_time_1_s"), delta_time_1_s);
        glUniform1f(glGetUniformLocation(mainShader.Program, "period_s"), period_s);
        glUniform4f(glGetUniformLocation(mainShader.Program, "color_bg"), color_bg[0], color_bg[1], color_bg[2], color_bg[3]);
        glUniform1i(glGetUniformLocation(mainShader.Program, "num_lines"), num_handlers);

        glBindBuffer(GL_TEXTURE_BUFFER, ssbo_params);
        glBufferSubData(GL_TEXTURE_BUFFER, 0, num_handlers * sizeof(LineParams), params);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_BUFFER, tex_buf_params);
        glTexBuffer(GL_TEXTURE_BUFFER, GL_R32F, ssbo_params);
        glUniform1i(glGetUniformLocation(mainShader.Program, "params"), 0);

        // ---------------

        glBindBuffer(GL_TEXTURE_BUFFER, ssbo_data);
        glBufferSubData(GL_TEXTURE_BUFFER, 0, total_length * sizeof(GLfloat), results_concat);

        glActiveTexture(GL_TEXTURE0 + 1);
        glBindTexture(GL_TEXTURE_BUFFER, tex_buf_data);
        glTexBuffer(GL_TEXTURE_BUFFER, GL_R32F, ssbo_data);
        glUniform1i(glGetUniformLocation(mainShader.Program, "data_samples"), 1);

        // ---------------

        glBindBuffer(GL_TEXTURE_BUFFER, ssbo_aux_data);
        glBufferSubData(GL_TEXTURE_BUFFER, 0, aux_buffer_total_length * sizeof(GLfloat), aux_buffers_concat);

        glActiveTexture(GL_TEXTURE0 + 2);
        glBindTexture(GL_TEXTURE_BUFFER, tex_buf_aux_data);
        glTexBuffer(GL_TEXTURE_BUFFER, GL_R32F, ssbo_aux_data);
        glUniform1i(glGetUniformLocation(mainShader.Program, "data_samples_aux"), 2);


        glfw_render_texture();

        GLenum err = glGetError();
        if (err != GL_NO_ERROR) {
            std::cout << "Error: " << err << std::endl;
        }
        /////////////////////////////////// IMGUI INTERFACE /////////////////////////////////////////////////////////////////////////////
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        // ImGUI window creation
        ImGui::Begin("Performance");
        ImGui::SliderFloat("timescale", &time_scale, 1.0, 128000.0);
        ImGui::SliderFloat("pattern scale", &pattern_scale, 1.0, 20000.0);
        ImGui::SliderFloat("movement scale", &movement_scale, 1.0, 20000.0);

        // Ends of imgui
        ImGui::End();
        // Renders the ImGUI elements
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

        // Check is an I/O event is happening
        glfwPollEvents();
        // Swapping back and front buffers
        glfwSwapBuffers(window);
        delete[] results_concat;
        delete[] aux_buffers_concat;

        auto now = clk::now();
        frame_counter++;
        frame_us_acc += time_diff_us(last_frame, now);
        if (time_diff_us(last_print, now) / 1000 >= print_interval_ms) {
            double frame_avg_us = frame_us_acc / frame_counter;
            frame_us_acc = 0;
            last_print = now;
            std::cout << "Processed in " << std::setw(10) << frame_avg_us << " us | "
                << std::setw(8) << std::fixed << std::setprecision(2)
                << frame_avg_us / frame_us_nominal * 100 << "% for " << target_fps << "FPS | \t"
                << "BPM: " << tempo_estimate << std::endl;
            frame_counter = 0;
        }
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


    printf("\n\nStopping visualization threads\n");
    for (size_t i = 0; i < num_handlers; i++) {
        handlers[i]->stop_thread();
    }

    printf("\n\nStopping audio stream and deconstructing.\n");
    ringBuffer->drain();
    stop_audio_stream();

    delete ringBuffer;
    delete[] mono;

    return 0;
}

int main (int argc, char** argv) {
    std::cout << "Starting sloth3 realtime audio visualizer..." << std::endl;

    using namespace audio;
    sdl_init();

    uint16_t device_id = 0;
    if (argc > 1) {
	   device_id = atoi(argv[1]);
    } else {
        auto device_names = get_audio_device_names();
        std::cout << "\nNo audio device specified. Please choose one!" << std::endl;
        std::cout << "Usage: \"./sloth3 <device_id>\"\n" << std::endl;
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

    const static unsigned int target_fps = 60;
    const static double update_interval_ms = 1000.0 / ((double) target_fps);
    const static double window_length_ms = 100;
    const static double print_interval_ms = 2000;
    const static int num_buffers_delay = 1;//20;

    size_t window_length_samples = window_length_ms / 1000 * spec.freq;
    spec.samples = (size_t) update_interval_ms / 1000.0 * spec.freq;

    BPSW_Spec params {
        .win_length_samples = window_length_samples,
        .update_length_samples = spec.samples,
        .win_window_fn = true,
        .adaptive_crop = false,
        .fft_dispersion = 2.1343,
        .fft_phase = BPSW_Phase::Constant,
        .fft_phase_const = 2.14313,
        .crop_length_samples = window_length_samples,
        .crop_offset = 0,
        .c_rad_base = 0.6,
        .c_rad_extr = 0.6,
        .color_inner = {0.03529411764705882, 0.20392156862745098, 0.48627450980392156, 1.0}
    };
    size_t c_length = params.win_length_samples / 2 + 1;
    double* freq_weighing = new double[c_length];
    for (size_t i = 0; i < c_length; i++) {
        freq_weighing[i] = i < 10 ? 1.5 :
                           i < 40 ? 1 : 0.05;
    }
    params.fft_freq_weighing = freq_weighing;


    BPSW_Spec params_inner {
        .win_length_samples = window_length_samples,
        .update_length_samples = spec.samples,
        .win_window_fn = true,
        .adaptive_crop = false,
        .fft_dispersion = -0.1, // -0.1
        .fft_phase = BPSW_Phase::Standing,
        .fft_phase_const = 0.8,
        .crop_length_samples = (window_length_samples) - 800,
        .crop_offset = 400,
        .c_rad_base = 0.3,
        .c_rad_extr = 1.8,
        .color_inner = {0.9803921568627451, 0.6509803921568628, 0.07450980392156863, 1.0}
    };
    size_t c_length_i = params_inner.win_length_samples / 2 + 1;
    double* freq_weighing_inner = new double[c_length_i];
    for (size_t i = 0; i < c_length_i; i++) {
        freq_weighing_inner[i] = i < 200 ? 0 : ( i > (c_length_i - 1200) ? 0 : 1);
    }
    params_inner.fft_freq_weighing = freq_weighing_inner;


    /* -------------------- CONFIGURATION END ----------------------------- */

    printf("Instantiating visualizations\n");
    BandpassStandingWave bpsw {spec, params};
    BandpassStandingWave bpsw_inner {spec, params_inner};
    printf("Done\n");

    constexpr size_t num_handlers = 2;
    printf("Instantiating visualization handler\n");
    VisualizationHandler* handlers[num_handlers] = {&bpsw, &bpsw_inner/*, &bpsw2*/};
    printf("Done\n");

    std::cout << "Initializing BTrack with " << spec.samples << " samples" << std::endl;
    BTrack btrack(spec.freq, spec.samples / 2, spec.samples);

    int retval = sloth_mainloop<SampleT>(device_id, spec, btrack, num_buffers_delay, handlers, num_handlers, print_interval_ms, target_fps);
    std::cout << "Mainloop ended" << std::endl;
    delete[] freq_weighing;
    delete[] freq_weighing_inner;
    return retval;
}


void glfw_render_texture()
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