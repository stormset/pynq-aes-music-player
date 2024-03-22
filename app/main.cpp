#include <iostream>
#include <string>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <unistd.h>
#include <termios.h>
#include <csignal>
#include <memory>
#include <cstring>
#include <utility>

#include "aes.h"
#include "directory_navigator.h"

struct ui_thread_params {
    aes& aes_inst;
    int read_pipe_fd;
    std::string dir_name;
};

struct aes_thread_params {
    aes& aes_inst;
    const std::string input_path;
    const std::string output_path;
    void *output_buffer;
    size_t output_buffer_size;
    enum operation_t {ENCRYPT, DECRYPT} operation{};
    enum status_t {CIPHER_READY, CIPHER_BUSY, CIPHER_FAILED, DECIPHER_READY, DECIPHER_BUSY, DECIPHER_FAILED} status{};

    // IPC
    int write_pipe_fd;

    // mutual exclusion
    pthread_mutex_t aes_mutex{};
    pthread_cond_t aes_cond{};

    aes_thread_params(aes &aes, std::basic_string<char> input_path, std::basic_string<char> output_path,
                      void * output_buffer, size_t output_buffer_size, operation_t operation, int write_pipe_fd) :
            aes_inst(aes), input_path(std::move(input_path)), output_path(std::move(output_path)),
            output_buffer(output_buffer), output_buffer_size(output_buffer_size), operation(operation), write_pipe_fd(write_pipe_fd) {}
};

struct aes_thread_msg {
    enum result_t {CIPHER_SUCCESS, CIPHER_FAILED, DECIPHER_SUCCESS, DECIPHER_FAILED} result{};
    size_t str_len{};
};

// IPC global variables
int pipes[2]; // pipe for main thread UI thread communication

static void int_handler(int signum)
{
    if (signum == SIGINT)
        close(pipes[1]); // closing of write fd involves sending EOF
}

static void dma_complete_callback(bool is_success, void *callback_params) {
    aes_thread_params *params = (aes_thread_params *) callback_params;

    pthread_mutex_lock(&params->aes_mutex);

    if (is_success)
        params->status = (params->operation == aes_thread_params::ENCRYPT ?
                          aes_thread_params::CIPHER_READY : aes_thread_params::DECIPHER_READY);
    else
        params->status = (params->operation == aes_thread_params::ENCRYPT ?
                          aes_thread_params::CIPHER_FAILED : aes_thread_params::DECIPHER_FAILED);

    pthread_cond_signal(&params->aes_cond);
    pthread_mutex_unlock(&params->aes_mutex);
}

static unsigned short get_terminal_height() {
    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    return w.ws_row;
}

static void *aes_thread(void *args) {
    std::shared_ptr<aes_thread_params> *dyn_shared_ptr = static_cast<std::shared_ptr<aes_thread_params>*>(args);
    std::shared_ptr<aes_thread_params> shared_ptr = *dyn_shared_ptr; // copy original shared_pointer, it's reference count will increase
    delete dyn_shared_ptr; // release the dynamically allocated shared_pointer, it's reference count will decrease

    aes_thread_msg msg;
    std::string msg1, msg2;

    // init mutex & condition flag
    pthread_mutex_init(&shared_ptr->aes_mutex, nullptr);
    pthread_cond_init(&shared_ptr->aes_cond, nullptr);

    std::function<void(bool, void*)> cb = std::function<void(bool, void*)>(dma_complete_callback);
    uint32_t key[4] = {0xFFFFFFFF, 0xAAAAAAAA, 0xBBBBBBBB, 0xCCCCCCCC };
    pthread_mutex_lock(&shared_ptr->aes_mutex);
    shared_ptr->status = (shared_ptr->operation == aes_thread_params::ENCRYPT ? aes_thread_params::CIPHER_BUSY : aes_thread_params::DECIPHER_BUSY);
    pthread_mutex_unlock(&shared_ptr->aes_mutex);

    try {
        switch (shared_ptr->operation) {
            case aes_thread_params::ENCRYPT:
                shared_ptr->aes_inst.encrypt_file(key, shared_ptr->input_path, shared_ptr->output_path, &cb, shared_ptr.get());
                break;
            case aes_thread_params::DECRYPT:
                shared_ptr->aes_inst.decrypt_file(key, shared_ptr->input_path, shared_ptr->output_path, &cb, shared_ptr.get());
                break;
        }
    } catch (const std::exception &e) {
        msg1 = shared_ptr->input_path;
        msg2 = e.what();
        msg.result = (shared_ptr->operation == aes_thread_params::ENCRYPT ? aes_thread_msg::CIPHER_FAILED : aes_thread_msg::DECIPHER_FAILED);

        goto send_results;
    }

    pthread_mutex_lock(&shared_ptr->aes_mutex);

    while (shared_ptr->status == aes_thread_params::CIPHER_BUSY || shared_ptr->status == aes_thread_params::DECIPHER_BUSY) {
        int rc = pthread_cond_wait(&shared_ptr->aes_cond, &shared_ptr->aes_mutex);

        if (rc < 0) {
            msg1 = shared_ptr->input_path;
            msg2 = "pthread_cond_wait() returned a negative value";
            msg.result = (shared_ptr->operation == aes_thread_params::ENCRYPT ? aes_thread_msg::CIPHER_FAILED : aes_thread_msg::DECIPHER_FAILED);
            pthread_mutex_unlock(&shared_ptr->aes_mutex);

            goto send_results;
        }
    }
    pthread_mutex_unlock(&shared_ptr->aes_mutex);

    // set status in message to UI thread
    switch (shared_ptr->status) {
        case aes_thread_params::CIPHER_READY:
            msg.result = aes_thread_msg::CIPHER_SUCCESS;
            break;
        case aes_thread_params::CIPHER_FAILED:
            msg.result = aes_thread_msg::CIPHER_FAILED;
            break;
        case aes_thread_params::DECIPHER_READY:
            msg.result = aes_thread_msg::DECIPHER_SUCCESS;
            break;
        case aes_thread_params::DECIPHER_FAILED:
            msg.result = aes_thread_msg::DECIPHER_FAILED;
            break;
        default:
            msg.result = aes_thread_msg::CIPHER_FAILED;
            break;
    }

    // set input/output file paths in message to UI thread
    msg1 = shared_ptr->input_path;
    msg2 = shared_ptr->output_path;

send_results:
    size_t s1 = msg1.empty() ? 0 : sizeof(msg1.c_str()[0]) * (strlen(msg1.c_str()) + 1);
    size_t s2 = msg2.empty() ? 0 : sizeof(msg2.c_str()[0]) * (strlen(msg2.c_str()) + 1);
    msg.str_len = s1 + s2;

    write(shared_ptr->write_pipe_fd, &msg, sizeof(msg));
    if (s1 > 0)
        write(shared_ptr->write_pipe_fd, msg1.c_str(), s1);
    if (s2 > 0)
        write(shared_ptr->write_pipe_fd, msg2.c_str(), s2);
    close(shared_ptr->write_pipe_fd);

    pthread_exit(nullptr);
}

std::array<int, 2> aes_thread_start(aes &aes, aes_thread_params::operation_t operation, const std::string& input_path,
                                    const std::string& output_path = "", void * output_buffer = nullptr, size_t output_buffer_size = 0) {
    int p[2];
    // open pipe for UI thread <==> AES thread communication
    pipe(p);

    std::pair<int, int> pipe_fd(p[0], p[1]);

    // This is a rather ugly construct, and requires the client code (aes_thread) to free the resource (shared_ptr),
    // but it is required to pass a shared_ptr as void*, to make sure that the lifecycle of the shared_ptr,
    // and thus the contained object doesn't end before thread starts.
    // std::thread already handles this in a nice way (but posix threads are used)
    std::shared_ptr<aes_thread_params> sptr = std::make_shared<aes_thread_params>(
            aes,
            input_path,
            output_path,
            output_buffer,
            output_buffer_size,
            operation,
            p[1]);
    void* arg = new std::shared_ptr<aes_thread_params>(sptr);

    int ret;
    pthread_t aes_pthread;
    if ((ret = pthread_create(&aes_pthread, nullptr, aes_thread, arg))) {
        throw std::system_error(EAGAIN, std::generic_category(), "Failed to create AES thread");
    }

    return std::array<int, 2>{p[0], p[1]};
}

static void *ui_thread(void *args) {
    std::vector<std::array<int, 2>> aes_pipes;
    ui_thread_params *ui_params = (ui_thread_params *)args;
    aes_thread_msg aes_msg;
    fd_set read_fds;
    char stdin_buff[3];
    int ret;

    unsigned short prev_nr_rows, nr_rows = get_terminal_height();
    std::function is_file_pred = [](const directory_navigator::entry& e){ return !e.is_directory; };
    directory_navigator directory_navigator(ui_params->dir_name, is_file_pred, nr_rows - 7);

    bool redraw = true;
    while (true) {
        if (redraw) {
            directory_navigator.set_scroll_height(nr_rows - 7);

            std::cout << "\033[H\033[J";
            std::cout << "╔════════════════════════════════════════════════╗" << std::endl;
            std::cout << "║                                                ║" << std::endl;
            std::cout << "║                                                ║" << std::endl;
            std::cout << "╚════════════════════════════════════════════════╝" << std::endl;
        }


        std::cout << "\033[H";
        std::cout << "\x1b[B" << "\x1b[B" << "\x1b[B" << "\x1b[B";
        std::cout << "\033[J";
        std::cout << directory_navigator;

        auto selected = directory_navigator.get_current_entry();

        // TODO: check against FD_SETSIZE limit
        FD_ZERO(&read_fds);
        FD_SET(STDIN_FILENO, &read_fds);
        FD_SET(ui_params->read_pipe_fd, &read_fds);

        int max_fd = std::max(STDIN_FILENO, ui_params->read_pipe_fd);
        for(const auto& e : aes_pipes) {
            if (e[0] > max_fd)
                max_fd = e[0];
            FD_SET(e[0], &read_fds);
        }

        while (select(max_fd + 1, &read_fds, nullptr, nullptr, nullptr) == 0);

        if (FD_ISSET(ui_params->read_pipe_fd, &read_fds)) {
            // write channel used only to send EOF => close read fd and terminate thread
            close(ui_params->read_pipe_fd);

            // TODO: inform all aes_thread threads to gracefully exit (a new pipe, or shared variable)
            
            // close all pending aes_thread communication channels
            for(const auto& e : aes_pipes) {
                close(e[0]);
                close(e[1]);
            } 

            break;
        }

        auto it = aes_pipes.begin();
        int fd;
        while(it != aes_pipes.end()) {
            fd = it->at(0);
            if (FD_ISSET(fd, &read_fds)) {
                // read data sent by aes_thread
                if(read(fd, &aes_msg, sizeof(aes_msg)) > 0)
                {
                    std::string m1, m2;
                    // parse optional messages
                    if (aes_msg.str_len > 0) {
                        char *msg_buff = (char *) malloc(aes_msg.str_len);
                        if(read(fd, msg_buff, aes_msg.str_len) > 0)
                        {
                            m1 = std::string(msg_buff);
                            size_t m1_size = sizeof(char) * (strlen(m1.c_str()) + 1);
                            if (m1_size < aes_msg.str_len) {
                                m2 = std::string(msg_buff + m1_size);
                            }
                        }
                        free(msg_buff);
                    }

                    // handle aes_thread results
                    switch (aes_msg.result) {
                        case aes_thread_msg::CIPHER_SUCCESS: {
                            // set xattr of the temporary (encrypted) file to show it is encrypted
                            bool xattr_val = true;
                            if (setxattr(m2.c_str(), "user.is_encrypted", &xattr_val, sizeof(xattr_val), 0) == 0) {
                                // remove original file rename temporary (encrypted) file to the original name
                                rename(m2.c_str(), m1.c_str());
                            } else {
                                // remove temporary (encrypted) file, indicate error
                                remove(m2.c_str());
                            }
                            // reopen directory to reflect changes
                            directory_navigator.open_directory(ui_params->dir_name);
                            directory_navigator.set_entry_suffix(m1, "");
                            break;
                        }
                        case aes_thread_msg::CIPHER_FAILED:
                            directory_navigator.set_entry_suffix(m1, "[" + m2 + "]");
                            break;
                        case aes_thread_msg::DECIPHER_SUCCESS:
                            // remove original (encrypted) file and rename temporary to original
                            rename(m2.c_str(), m1.c_str());
                            // reopen directory to reflect changes
                            directory_navigator.open_directory(ui_params->dir_name);
                            directory_navigator.set_entry_suffix(m1, "");
                            break;
                        case aes_thread_msg::DECIPHER_FAILED:
                            directory_navigator.set_entry_suffix(m1, "[ " + m2 + "]");
                            break;
                    }
                }
                close(fd);
                it = aes_pipes.erase(it);
            } else ++it;
        }

        if (FD_ISSET(STDIN_FILENO, &read_fds)) {
            // read user input from stdin
            if ((ret = read(STDIN_FILENO, stdin_buff, sizeof(stdin_buff))) > 0 && selected.has_value()) {
                const directory_navigator::entry &selected_entry = selected.value().get();

                // check if terminal height changed
                prev_nr_rows = nr_rows;
                nr_rows = get_terminal_height();
                redraw = nr_rows != prev_nr_rows;

                // handle keyboard presses (arrows are three-character sequences)
                switch(ret > 2 ? stdin_buff[1] : stdin_buff[0]) {
                case 'e': case 'd': {
                        std::string tmp_name(tempnam(ui_params->dir_name.c_str(), selected_entry.path.c_str()));
                        aes_thread_params::operation_t operation = stdin_buff[0] == 'e' ? aes_thread_params::ENCRYPT : aes_thread_params::DECRYPT;
                        std::string state_msg(" [");

                        // check is_encrypted extended attribute
                        bool xattr_val;
                        ret = (int)getxattr(selected_entry.path.c_str(), "user.is_encrypted", &xattr_val, sizeof(xattr_val));

                        if ((operation == aes_thread_params::ENCRYPT && (ret < 0 || !xattr_val)) ||
                            ((operation == aes_thread_params::DECRYPT && ret > 0 && xattr_val))) {
                            state_msg.append(operation == aes_thread_params::ENCRYPT ? "encrypting..." : "decrypting...");
                            state_msg.append("] ");
                            directory_navigator.set_entry_suffix(selected_entry.path, state_msg);
                            try {
                                const auto& p = aes_thread_start(ui_params->aes_inst,
                                                                 operation,
                                                                 selected_entry.path,
                                                                 tmp_name);
                                aes_pipes.emplace_back(p); 
                            } catch (const std::system_error &e) {
                                state_msg = "[";
                                state_msg.append(e.what());
                                state_msg.append("]");
                                directory_navigator.set_entry_suffix(selected_entry.path, state_msg);
                            }
                        } else {
                            state_msg.append("File is already ");
                            state_msg.append(operation == aes_thread_params::ENCRYPT ? "encrypted." : "decrypted.");
                            state_msg.append("] ");
                            directory_navigator.set_entry_suffix(selected_entry.path, state_msg);
                        }

                        break;
                    }
                    case 0x5B:
                        if (ret > 2) {
                            switch (stdin_buff[2]) {
                                case 0x42:
                                    directory_navigator.relative_navigate_to_entry(directory_navigator::DOWN, is_file_pred);
                                    break;
                                case 0x41:
                                    directory_navigator.relative_navigate_to_entry(directory_navigator::UP, is_file_pred);
                                    break;
                                case 0x44: {
                                    std::cout << "VOL-";
                                    break;
                                }
                                case 0x43:
                                    std::cout << "VOL+";
                                    break;
                            }
                        }
                        break;
                }
            }
        }
    }

    pthread_exit(nullptr);
}

int main(int argc, char *argv[]) {
    aes aes;
    ui_thread_params ui_params {.aes_inst = aes};
    pthread_t ui_pthread;
    int ret;
    struct sigaction sa{};

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
    ui_params.dir_name = std::string(argv[1]);

    // initialize aes instance
    aes.init();

    // create pipe for communication with UI thread
    pipe(pipes);
    ui_params.read_pipe_fd = pipes[0];

    // setup SIGINT handler, used to inform UI thread to exit
    sa.sa_handler = int_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, nullptr);

    // start UI thread
    if ((ret = pthread_create(&ui_pthread, nullptr, ui_thread, &ui_params))) {
        std::cout << "Failed to create UI thread: pthread_create() returned " << ret;
        return -1;
    }

    // wait for ui thread to exit
    pthread_join(ui_pthread, nullptr);

    // restore terminal settings
    tcsetattr( STDIN_FILENO, TCSANOW, &old_term_settings);

    // exit
    pthread_exit(nullptr);
}