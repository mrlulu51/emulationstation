#include "UsbHandler.h"
#include "Log.h"
#include <SDL2/SDL_events.h>
#include <SDL2/SDL_stdinc.h>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <sys/poll.h>
#include <thread>

#ifdef __linux__
#include <cstring>
#include <libudev.h>
#include <poll.h>
#endif

uint32_t EVENT_USB_INSERTED = (uint32_t)-1;

UsbHandler::UsbHandler() : mRunning(false) {
  if (EVENT_USB_INSERTED == (uint32_t)-1) {
    EVENT_USB_INSERTED = SDL_RegisterEvents(1);
  }
}

UsbHandler::~UsbHandler() { stop(); }

void UsbHandler::start() {
  if (mRunning)
    return;
  mRunning = true;
  mThread = std::thread(&UsbHandler::run, this);
  LOG(LogInfo) << "UsbHandler::start UsbHandler initialized";
}

void UsbHandler::stop() {
  if (!mRunning)
    return;
  mRunning = false;
  if (mThread.joinable()) {
    mThread.join();
  }
}

std::string waitForMount(const std::string &devnode) {
  for (int i = 0; i < 10; i++) {
    std::ifstream mounts("/proc/mounts");
    std::string line;
    while (std::getline(mounts, line)) {
      if (line.find(devnode + " ") == 0) {
        std::istringstream iss(line);
        std::string dev, mountPoint;
        iss >> dev >> mountPoint;

        size_t pos;
        while ((pos = mountPoint.find("\\040")) != std::string::npos) {
          mountPoint.replace(pos, 4, " ");
        }
        return mountPoint;
      }
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
  }

  return "";
}

void UsbHandler::run() {
#ifdef __linux__
  struct udev *udev = udev_new();
  if (!udev) {
    LOG(LogError) << "UsbHandler: Unable to initialize libudev" << std::endl;
    return;
  }

  struct udev_monitor *mon = udev_monitor_new_from_netlink(udev, "udev");
  udev_monitor_filter_add_match_subsystem_devtype(mon, "block", NULL);
  udev_monitor_enable_receiving(mon);

  int fd = udev_monitor_get_fd(mon);
  struct pollfd fds[1];
  fds[0].fd = fd;
  fds[0].events = POLLIN;

  while (mRunning) {
    int ret = poll(fds, 1, 500);
    if (ret > 0 && (fds[0].revents & POLLIN)) {
      struct udev_device *dev = udev_monitor_receive_device(mon);
      if (dev) {
        const char *action = udev_device_get_action(dev);
        const char *id_bus = udev_device_get_property_value(dev, "ID_BUS");
        const char *devnode = udev_device_get_devnode(dev);

        if (action && std::string(action) == "add" && id_bus &&
            std::string(id_bus) == "usb" && devnode) {

          std::string nodePath(devnode);
          std::string mountPoint = waitForMount(nodePath);

          if (!mountPoint.empty()) {
            std::string targetFile = mountPoint + "/es_usb.txt";
            std::ifstream file(targetFile);

            if (file.is_open()) {
              std::string identifier;
              std::getline(file, identifier);

              if (EVENT_USB_INSERTED != (uint32_t)-1) {
                SDL_Event event;
                SDL_zero(event);
                event.type = EVENT_USB_INSERTED;

                char* idStr = new char[identifier.length() + 1];
                std::strcpy(idStr, identifier.c_str());
                event.user.data1 = idStr;

                SDL_PushEvent(&event);

                LOG(LogInfo) << "UsbHandler::run Usb was plugged in";
              }
            }
          }
        }

        udev_device_unref(dev);
      }
    }
  }

  udev_monitor_unref(mon);
  udev_unref(udev);
#else
  while (mRunning) {
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
  }
#endif
}