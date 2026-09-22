#pragma once

#include <SDL2/SDL.h>
#include <sys/types.h>
#include <thread>
#include <atomic>

extern uint32_t EVENT_USB_INSERTED;

class UsbHandler {
    public:
        UsbHandler();
        ~UsbHandler();

        void start();

        void stop();
    
    private:
        void run();

        std::thread mThread;
        std::atomic<bool> mRunning;
};