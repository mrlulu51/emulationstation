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

UsbHandler::UsbHandler() : mRunning(false)
{
  if (EVENT_USB_INSERTED == (uint32_t)-1)
  {
    EVENT_USB_INSERTED = SDL_RegisterEvents(1);
  }
}

UsbHandler::~UsbHandler() { stop(); }

void UsbHandler::start()
{
  if (mRunning)
    return;
  mRunning = true;
  mThread = std::thread(&UsbHandler::run, this);
  LOG(LogInfo) << "UsbHandler::start UsbHandler initialized";
}

void UsbHandler::stop()
{
  if (!mRunning)
    return;
  mRunning = false;
  if (mThread.joinable())
  {
    mThread.join();
  }
}

std::string waitForMount(const std::string &devnode)
{
  for (int i = 0; i < 10; i++)
  {
    std::ifstream mounts("/proc/mounts");
    std::string line;
    while (std::getline(mounts, line))
    {
      if (line.find(devnode + " ") == 0)
      {
        std::istringstream iss(line);
        std::string dev, mountPoint;
        iss >> dev >> mountPoint;

        size_t pos;
        while ((pos = mountPoint.find("\\040")) != std::string::npos)
        {
          mountPoint.replace(pos, 4, " ");
        }
        return mountPoint;
      }
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
  }

  return "";
}

void UsbHandler::run()
{
#ifdef __linux__
  struct udev *udev = udev_new();
  if (!udev)
  {
    LOG(LogError) << "UsbHandler: Unable to initialize libudev" << std::endl;
    return;
  }

  struct udev_enumerate *enumerate = udev_enumerate_new(udev);
  udev_enumerate_add_match_subsystem(enumerate, "block");
  udev_enumerate_scan_devices(enumerate);

  struct udev_list_entry *devices = udev_enumerate_get_list_entry(enumerate);
  struct udev_list_entry *dev_list_entry;

  udev_list_entry_foreach(dev_list_entry, devices)
  {
    const char *path = udev_list_entry_get_name(dev_list_entry);
    struct udev_device *dev = udev_device_new_from_syspath(udev, path);
    if (dev)
    {
      const char *id_bus = udev_device_get_property_value(dev, "ID_BUS");
      const char *devnode = udev_device_get_devnode(dev);

      if (id_bus && std::string(id_bus) == "usb" && devnode)
      {
        processDeviceNode(devnode);
      }
      udev_device_unref(dev);
    }
  }
  udev_enumerate_unref(enumerate);

  struct udev_monitor *mon = udev_monitor_new_from_netlink(udev, "udev");

  if (!mon)
  {
    LOG(LogError) << "UsbHandler: failed to create udev monitor";
    udev_unref(udev);
    return;
  }

  int filterResult =
      udev_monitor_filter_add_match_subsystem_devtype(
          mon, "block", NULL);

  LOG(LogDebug) << "UsbHandler: monitor filter result = "
                << filterResult;

  if (filterResult < 0)
  {
    LOG(LogError) << "UsbHandler: failed to add block filter";
    udev_monitor_unref(mon);
    udev_unref(udev);
    return;
  }

  int receiveResult = udev_monitor_enable_receiving(mon);

  LOG(LogDebug) << "UsbHandler: monitor enable result = "
                << receiveResult;

  if (receiveResult < 0)
  {
    LOG(LogError) << "UsbHandler: failed to enable udev monitor";
    udev_monitor_unref(mon);
    udev_unref(udev);
    return;
  }

  int fd = udev_monitor_get_fd(mon);

  if (fd < 0)
  {
    LOG(LogError) << "UsbHandler: invalid udev monitor fd";
    udev_monitor_unref(mon);
    udev_unref(udev);
    return;
  }

  struct pollfd fds[1];
  fds[0].fd = fd;
  fds[0].events = POLLIN;

  LOG(LogDebug) << "UsbHandler: udev monitor started, fd=" << fd;

  while (mRunning)
  {
    int ret = poll(fds, 1, 500);
    LOG(LogDebug) << "UsbHandler: poll returned " << ret;
    if (ret > 0 && (fds[0].revents & POLLIN))
    {
      struct udev_device *dev = udev_monitor_receive_device(mon);
      if (dev)
      {
        const char *action = udev_device_get_action(dev);
        const char *devnode = udev_device_get_devnode(dev);
        const char *subsystem = udev_device_get_subsystem(dev);
        const char *devtype = udev_device_get_devtype(dev);

        LOG(LogDebug) << "UsbHandler: udev event"
                      << " action=" << (action ? action : "NULL")
                      << " subsystem=" << (subsystem ? subsystem : "NULL")
                      << " devtype=" << (devtype ? devtype : "NULL")
                      << " devnode=" << (devnode ? devnode : "NULL");

        if (action && std::string(action) == "add" &&
            subsystem && std::string(subsystem) == "block" &&
            devnode)
        {

          struct udev_device *parent_usb = udev_device_get_parent_with_subsystem_devtype(dev, "usb", "usb_device");
          if (parent_usb)
          {
            LOG(LogDebug) << "UsbHandler: USB block device detected: "
                          << devnode;
            processDeviceNode(devnode);
          }
          else
          {
            LOG(LogDebug) << "UsbHandler: block device is not USB: "
                          << devnode;
          }
        }

        udev_device_unref(dev);
      }
    }
  }

  udev_monitor_unref(mon);
  udev_unref(udev);
#else
  while (mRunning)
  {
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
  }
#endif
}

void UsbHandler::processDeviceNode(const std::string &devnode)
{
  std::string nodePath(devnode);
  std::string mountPoint = waitForMount(nodePath);

  if (!mountPoint.empty())
  {
    std::string playerDataFile = mountPoint + "/ESGI-Game/player_data.json";
    std::ifstream file(playerDataFile);

    if (file.is_open())
    {
      std::stringstream buffer;
      buffer << file.rdbuf();
      std::string jsonContent = buffer.str();

      if (EVENT_USB_INSERTED != (uint32_t)-1)
      {
        SDL_Event event;
        SDL_zero(event);
        event.type = EVENT_USB_INSERTED;

        char *dataStr = new char[jsonContent.length() + 1];
        std::strcpy(dataStr, jsonContent.c_str());
        event.user.data1 = dataStr;

        char *mountStr = new char[mountPoint.length() + 1];
        std::strcpy(mountStr, mountPoint.c_str());
        event.user.data2 = mountStr;

        SDL_PushEvent(&event);
      }
    }
  }
}