#include <opencv2/opencv.hpp>
#include <iostream>
#include <chrono>
#include <algorithm>
#include <string>
#include <thread>
#include <vector>
#include <fstream>
#include <cstdlib>
#include <filesystem>

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
#else
    #include <sys/ioctl.h>
    #include <unistd.h>
#endif

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#define RESET_CURSOR "\033[H"
#define HIDE_CURSOR  "\033[?25l"
#define SHOW_CURSOR  "\033[?25h"

void getTerminalSize(int& width, int& height);
cv::Mat scaleFrame(const cv::Mat& input, int targetW, int targetH);
void genFrames(cv::VideoCapture cap, const int width, const int height, const int total_frames, const std::string gscale);
std::string getValueFromFile(std::string search, std::vector<std::string> fileContent);

std::vector<std::string> frame_buffer;

int main() {
    int choice1;
    std::cout << "(1) Convert mp4\n(2) Play from file\n(3) Exit\n";
    std::cin >> choice1;
    std::string gscale = "$@B%8&WM#*oahkbdpqwmZO0QLCJUYXzcvunxrjft/\\|()1{}[]?-_+~i!lI;:,\"^`'. ";
    int w, h;
    getTerminalSize(w, h);
    if (choice1 == 1) {
        std::string mp4_path;
        std::cout << "Path to mp4: ";
        std::cin >> mp4_path;

        std::filesystem::path mp4(mp4_path);
        std::string mp4_parent_dir = std::filesystem::absolute(mp4).parent_path().string();
        std::string mp4_title = mp4.stem();
        std::string path = mp4_parent_dir + "/" + mp4_title;
        std::string frame_buffer_path = path + ".txt";
        std::filesystem::path mp3_path(path + ".mp3");

        std::ofstream frame_buffer_file(mp4_parent_dir + "/" + mp4_title + ".txt");
        cv::VideoCapture cap(mp4_path);
        if (!cap.isOpened()) {
            std::cerr << "Error: Could not open video file\n";
            return -1;
        }
        int frame_count = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_COUNT));
        double fps = cap.get(cv::CAP_PROP_FPS);
        genFrames(cap, w, h, frame_count, gscale);
        std::string save_text = "";
        save_text += "@@@terminal_width=" + std::to_string(w) + "\n@@@terminal_height=" + std::to_string(h) + "\n@@@frames=" + std::to_string(frame_count) + "\n@@@fps=" + std::to_string(fps);
        int cnt = 0;
        for (auto i : frame_buffer) {
            save_text += "\n@@@frame" + std::to_string(cnt) + "=" + i;
            cnt++;
        }
        frame_buffer_file << save_text << std::endl;
        frame_buffer_file.close();
        cap.release();
        std::cout << "\nFrames have been converted successfully\n";
        std::cout << "Frame buffer data saved to: " << frame_buffer_path << "\n";
        std::cout << "To get the mp3, run this command in your terminal: " << "ffmpeg -i " << std::filesystem::absolute(mp4) << " -vn -acodec libmp3lame -q:a 2 " << mp3_path << "\n";
    } else if (choice1 == 2) {
        std::string frame_path;
        std::string mp3_path;
        std::cout << "Path to frame data file (<title>.txt): ";
        std::cin >> frame_path;
        std::cout << "Path to mp3 (<title>.mp3): ";
        std::cin >> mp3_path;
        std::ifstream frame_buffer_file(frame_path);
        std::string line;

        std::string file_width = "@@@terminal_width=";
        std::string file_height = "@@@terminal_height=";
        std::string file_frames = "@@@frames=";
        std::string file_fps = "@@@fps=";
        int saved_w = -1;
        int saved_h = -1;
        int saved_frames = -1;
        double saved_fps = -1;
    
        int cnt = 1;
        while (std::getline(frame_buffer_file, line)) {
            int res_w = line.find(file_width);
            int res_h = line.find(file_height);
            int res_frames = line.find(file_frames);
            int res_fps = line.find(file_fps);
            if (res_w != std::string::npos && saved_w == -1) {
                line.erase(res_w, file_width.length());
                saved_w = std::stoi(line);
            } else if (res_h != std::string::npos && saved_h == -1) {
                line.erase(res_h, file_height.length());
                saved_h = std::stoi(line);
            } else if (res_frames != std::string::npos && saved_frames == -1) {
                line.erase(res_frames, file_frames.length());
                saved_frames = std::stoi(line);
            } else if (res_fps != std::string::npos && saved_fps == -1) {
                line.erase(res_fps, file_fps.length());
                saved_fps = std::stod(line);
            }
        }

        if (saved_w == -1 || saved_h == -1 || saved_frames == -1 || saved_fps == -1) {
            std::cerr << "Error: Invalid frame data file\n";
            std::exit(EXIT_FAILURE);
        }

        if (saved_w != w or saved_h != h) {
            std::cerr << "Error: Terminal width or height changed, try regenerating the video\n";
            return -1;
        }

        // what i need to do:
        // for loop, inside we get the line which contains the frame, but we have to remember what the last
        // 'identifier' was. for example: @@@frame0=somestring, we would have to remember '0' because frame 0,
        // and every newline without an 'identifier' we would have to assign it to that
        frame_buffer_file.clear();
        frame_buffer_file.seekg(0);

        int current_frame = 0;
        cnt = 0;
        int saved_frame_lines = 0;
        bool found = false;
        std::string file_frame = "@@@frame";
        std::string read_frame = "";
        while (std::getline(frame_buffer_file, line)) {
            // we just have a counter that counts how many loops and then it searches for
            // '@@@frame{index}='
            file_frame = "@@@frame" + std::to_string(current_frame) + "=";
            if (!found) {
                int res = line.find(file_frame);
                if (res != std::string::npos) {
                    found = true;
                    line.erase(res, file_frame.length());
                    read_frame += line;
                    saved_frame_lines++;
                }
            } else {
                if (saved_frame_lines < h) {
                    read_frame += line;
                    saved_frame_lines++;
                } else if (saved_frame_lines == h) {
                    read_frame += line;
                    frame_buffer.push_back(read_frame);
                    found = false;
                    current_frame++;
                    saved_frame_lines = 0;
                    read_frame = "";
                }
            }
            cnt++;
        }

        if (frame_buffer.empty()) {
            std::cerr << "Error: Invalid frame data file\n";
            std::exit(EXIT_FAILURE);
        }

        frame_buffer_file.close();

        ma_engine engine;
        if (ma_engine_init(NULL, &engine) != MA_SUCCESS) {
            std::cerr << "Error: Engine could not be started\n";
            return -1;
        }

        ma_engine_play_sound(&engine, mp3_path.c_str(), NULL);

        auto start_time = std::chrono::steady_clock::now();

        int target_frame = 0;

        std::cout << HIDE_CURSOR;

        while (true) {
            auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start_time).count() / 1000000.0;

            target_frame = static_cast<int>(elapsed * saved_fps);

            if (target_frame >= frame_buffer.size()) break;
            if (target_frame < 0) target_frame = 0;
            std::cout << RESET_CURSOR << frame_buffer[target_frame] << "\r" << std::flush; // not sure about the target_frame - 1, still gotta test
        }

        std::cout << SHOW_CURSOR;
        ma_engine_uninit(&engine);
    }

    return 0;
}

void getTerminalSize(int& width, int& height) {
    #ifdef _WIN32
        CONSOLE_SCREEN_BUFFER_INFO csbi;
        GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
        width = csbi.srWindow.Right - csbi.srWindow.Left + 1;
        height = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
    #else
        struct winsize w;
        ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
        width = w.ws_col;
        height = w.ws_row;
    #endif
}

cv::Mat scaleFrame(const cv::Mat& input, int targetW, int targetH) {
    double scale = std::min((double) targetW / input.cols, (double) targetH / input.rows);

    int newW = static_cast<int>(input.cols * 2 * scale);
    int newH = static_cast<int>(input.rows * scale);

    cv::Mat resized;
    cv::resize(input, resized, cv::Size(newW, newH), 0, 0, cv::INTER_LINEAR);

    int top = (targetH - newH) / 2;
    int bottom = targetH - newH - top;
    int left = (targetW - newW) / 2;
    int right = targetW - newW - left;

    cv::Mat finalImage;

    cv::copyMakeBorder(resized, finalImage, top, bottom, left, right, cv::BORDER_CONSTANT, cv::Scalar(0, 0, 0));

    return finalImage;
}

void genFrames(cv::VideoCapture cap, const int w, const int h, const int total_frames, const std::string gscale) {
    double progress = 0.0;
    std::cout << HIDE_CURSOR;
    for (int k = 0; k < total_frames; k++) {
        std::string current_frame = "";

        std::string progress_bar = "[";
        int pos = (w / 2) * progress;
        for (int i = 0; i < (w / 2); ++i) {
            if (i < pos) progress_bar += "=";
            else if (i == pos) progress_bar += ">";
            else progress_bar += " ";
        }
        
        progress_bar += "] " + std::to_string(int(progress * 100.0)) + " % (" + std::to_string(k + 1) + "/" + std::to_string(total_frames) + ")\r";

        std::cout << progress_bar << std::flush;

        // frame 'generate' logic
        cap.set(cv::CAP_PROP_POS_FRAMES, k);
        cv::Mat frame;
        bool ret = cap.read(frame);
        if (!ret) {
            std::cerr << "End of video or error" << std::endl;
            break;
        }

        cv::Mat grayscale;
        cvtColor(frame, grayscale, cv::COLOR_RGB2GRAY);

        cv::Mat finalFrame = scaleFrame(grayscale, w, h);

        for (int i = 0; i < finalFrame.rows; i++) {
            std::string text = "";
            for (int j = 0; j < finalFrame.cols; j++) {
                int pixel = (int)finalFrame.at<uchar>(i, j);
                text += gscale[(gscale.length() - 1) - (pixel % gscale.length())];
            }
            current_frame += text;
            if (i < finalFrame.rows) {
                current_frame += "\n";
            }
        }

        frame_buffer.push_back(current_frame);
 
        if (k != total_frames - 2) progress += 1.0 / total_frames;
        else progress = 1.0;
    }
}
