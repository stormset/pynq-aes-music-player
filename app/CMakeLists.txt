cmake_minimum_required(VERSION 3.27)
project(aes_music_player_app)

set(CMAKE_CXX_STANDARD 17)


add_executable(aes_music_player_app main.cpp
        directory_navigator.cpp
        directory_navigator.h
        aes.cpp
        aes.h
        util.cpp
        util.h
)
target_include_directories(aes_music_player_app PRIVATE include)
