#ifndef AES_MUSIC_PLAYER_APP_PTHREAD_WRAPPER_H
#define AES_MUSIC_PLAYER_APP_PTHREAD_WRAPPER_H


#include <csignal>
#include <thread>

class pthread_wrapper {
public:
    pthread_wrapper() = default;
    virtual ~pthread_wrapper() = default;

    bool start() {
        return (pthread_create(&_thread, nullptr, wrapped_run, this) == 0);
    }

    void join() const {
        pthread_join(_thread, nullptr);
    }

protected:
    virtual void run() = 0;

private:
    static void *wrapped_run(void *instance_ptr) { ((pthread_wrapper *) instance_ptr)->run(); return nullptr;}

    pthread_t _thread{};
};


#endif //AES_MUSIC_PLAYER_APP_PTHREAD_WRAPPER_H
