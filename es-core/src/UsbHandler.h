#pragma once

#include <SDL2/SDL.h>
#include <sys/types.h>
#include <thread>
#include <atomic>

extern uint32_t EVENT_USB_INSERTED;
extern uint32_t EVENT_USB_UNPLUGGED;

struct UsbInsertedEventData {
    char* devnode;
    char* mountPoint;
};

class UsbHandler {
    public:
        UsbHandler();
        ~UsbHandler();

        void start();
        void stop();
    
    private:
        void run();
        void processDeviceNode(const std::string& devnode);
        void processDeviceRemoval(const std::string& devnode);

        std::thread mThread;
        std::atomic<bool> mRunning;
};