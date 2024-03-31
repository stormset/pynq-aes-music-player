#include <iostream>
#include <string>
#include <unistd.h>
#include <termios.h>
#include <csignal>
#include <memory>
#include <fstream>

#include "aes.h"
#include "ui_thread.h"
#include "player_thread.h"

// IPC global variables
int main_to_ui_write_pipe_fd; // pipe for main thread to UI thread communication

static void int_handler(int signum)
{
    if (signum == SIGINT)
        close(main_to_ui_write_pipe_fd); // closing of write fd involves sending EOF
}

int main(int argc, char *argv[]) {
    // create AES instance
    aes aes;
    // pipe between main and UI thread
    int main_to_ui_pipe[2];
    int ui_to_player_pipe[2];
    int player_to_ui_pipe[2];

    // setup terminal so every input isn't buffered until RETURN is pressed (canonical mode)
    // see: https://stackoverflow.com/a/1798833
    static struct termios old_term_settings, new_term_settings;
    tcgetattr( STDIN_FILENO, &old_term_settings);
    new_term_settings = old_term_settings;
    new_term_settings.c_lflag &= ~(ICANON | ECHO);
    tcsetattr( STDIN_FILENO, TCSANOW, &new_term_settings);

    // get directory is passed in args
    if (argc < 2 || !argv[1]) {
        std::cout << "You must type a directory name" << std::endl;
        return -1;
    }
    std::string path(argv[1]);

    // initialize aes instance
    aes.init();

    // create pipe for UI thread <=> main simplex communication
    if (pipe(main_to_ui_pipe) != 0) {
        std::cout << "Failed to open pipe between UI and main threads." << std::endl;
        return -1;
    }


    // create 2 pipes for UI thread <=> player_thread full-duplex communication
    if (pipe(ui_to_player_pipe) != 0) {
        std::cout << "Failed to open pipe between UI and player threads." << std::endl;
        return -1;
    }

    if (pipe(player_to_ui_pipe) != 0) {
        std::cout << "Failed to open pipe between player and UI threads." << std::endl;
        return -1;
    }
    main_to_ui_write_pipe_fd = main_to_ui_pipe[1];

    // setup SIGINT handler, used to inform UI thread to exit
    struct sigaction sh{};
    sh.sa_handler = int_handler;
    sigemptyset(&sh.sa_mask);
    sh.sa_flags = 0;
    sigaction(SIGINT, &sh, nullptr);

    // start player thread
    player_thread player(ui_to_player_pipe[0], player_to_ui_pipe[1]);
    if (!player.start()) {
        std::cout << "Failed creating player thread." << std::endl;
        return -1;
    }

    // start UI thread
    ui_thread ui(aes, path, main_to_ui_pipe[0], player_to_ui_pipe[0], ui_to_player_pipe[1]);
    if (!ui.start()) {
        std::cout << "Failed creating UI thread." << std::endl;
        return -1;
    }

    // wait for player thread to exit
    player.join();
    // wait for ui thread to exit
    ui.join();

    // restore terminal settings
    tcsetattr( STDIN_FILENO, TCSANOW, &old_term_settings);

    // exit
    pthread_exit(nullptr);
}
