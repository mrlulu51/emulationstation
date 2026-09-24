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
uint32_t EVENT_USB_UNPLUGGED = (uint32_t)-1;

UsbHandler::UsbHandler() : mRunning(false)
{
  if (EVENT_USB_INSERTED == (uint32_t)-1)
    EVENT_USB_INSERTED = SDL_RegisterEvents(1);

  if (EVENT_USB_UNPLUGGED == (uint32_t)-1)
    EVENT_USB_UNPLUGGED = SDL_RegisterEvents(1);
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

        LOG(LogDebug)
            << "UsbHandler: device "
            << devnode
            << " mounted at "
            << mountPoint;
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

    if (!dev)
      continue;

    const char *devnode = udev_device_get_devnode(dev);
    const char *devtype = udev_device_get_devtype(dev);

    struct udev_device *parent_usb = udev_device_get_parent_with_subsystem_devtype(dev, "usb", "usb_device");

    if (devnode && devtype && std::string(devtype) == "partition" && parent_usb)
    {
      LOG(LogDebug)
          << "UsbHandler: existing USB partition detected: "
          << devnode;
      processDeviceNode(devnode);
    }

    udev_device_unref(dev);
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

  if (filterResult < 0)
  {
    LOG(LogError) << "UsbHandler: failed to add block filter";
    udev_monitor_unref(mon);
    udev_unref(udev);
    return;
  }

  int receiveResult = udev_monitor_enable_receiving(mon);

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

  LOG(LogDebug)
      << "UsbHandler: udev monitor started, fd="
      << fd;

  while (mRunning)
  {
    int ret = poll(fds, 1, 500);

    if (ret < 0)
    {
      if (!mRunning)
        break;

      continue;
    }

    if (ret == 0)
      continue;

    if (!(fds[0].revents & POLLIN))
      continue;

    struct udev_device *dev = udev_monitor_receive_device(mon);

    if (!dev)
      continue;

    const char *action = udev_device_get_action(dev);
    const char *devnode = udev_device_get_devnode(dev);
    const char *devtype = udev_device_get_devtype(dev);

    struct udev_device *parent_usb = udev_device_get_parent_with_subsystem_devtype(dev, "usb", "usb_device");
    bool is_usb = (parent_usb != nullptr);

    LOG(LogDebug)
        << "UsbHandler: udev event"
        << " action="
        << (action ? action : "NULL")
        << " devnode="
        << (devnode ? devnode : "NULL")
        << " devtype="
        << (devtype ? devtype : "NULL")
        << " usb="
        << (parent_usb ? "yes" : "no");

    if (action && devnode)
    {
      std::string actionStr(action);

      if (is_usb && std::string(devtype) == "partition")
      {
        if (actionStr == "add")
        {
          processDeviceNode(devnode);
        }
        else if (actionStr == "remove")
        {
          processDeviceRemoval(devnode);
        }
      }
    }

    udev_device_unref(dev);
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
  std::string mountPoint = waitForMount(devnode);

  if (mountPoint.empty())
    return;

  std::string playerDataFile = mountPoint + "/ESGI-Game/player_data.json";
  std::ifstream file(playerDataFile);

  if (!file.is_open())
    return;

  file.close();

  if (EVENT_USB_INSERTED == (uint32_t)-1)
    return;

  LOG(LogDebug)
      << "UsbHandler: preparing EVENT_USB_INSERTED for "
      << mountPoint;

  SDL_Event event;
  SDL_zero(event);

  event.type = EVENT_USB_INSERTED;

  UsbInsertedEventData* data = new UsbInsertedEventData;

  data->devnode = new char[devnode.size() + 1];
  std::strcpy(data->devnode, devnode.c_str());

  data->mountPoint = new char[mountPoint.length() + 1];
  std::strcpy(data->mountPoint, mountPoint.c_str());
  event.user.data1 = data;
  event.user.data2 = nullptr;

  if (SDL_PushEvent(&event) != 1)
  {
    LOG(LogError)
        << "UsbHandler: SDL_PushEvent failed: "
        << SDL_GetError();
    delete[] data;
  }
  else
  {
    LOG(LogDebug)
        << "UsbHandler: EVENT_USB_INSERTED pushed successfully";
  }
}

void UsbHandler::processDeviceRemoval(const std::string &devnode)
{
  if (EVENT_USB_UNPLUGGED == (uint32_t)-1)
    return;

  SDL_Event event;
  SDL_zero(event);

  event.type = EVENT_USB_UNPLUGGED;

  char *devnodeStr = new char[devnode.length() + 1];
  std::strcpy(devnodeStr, devnode.c_str());

  event.user.data1 = devnodeStr;
  event.user.data2 = nullptr;

  if (SDL_PushEvent(&event) != 1)
    delete[] devnodeStr;
}