#pragma once

#include <string>
#include <map>

class Window;

class UsbEventHandler
{
    public:
        static void handleInserted(
            Window* window,
            const std::string& mountPoint,
            const std::string& devnode
        );

        static void handleUnplugged(
            Window* window,
            const std::string& devnode
        );
    
    private:
        static std::map<std::string, std::string> sUsbDevices;
};