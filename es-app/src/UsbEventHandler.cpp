#include "UsbEventHandler.h"

#include "Log.h"
#include "PlayerData.h"
#include "ProfileManager.h"

#include "components/HelpComponent.h"
#include "guis/GuiProfileSelector.h"
#include "Window.h"

#include "InputManager.h"

#include <map>

std::map<std::string, std::string> UsbEventHandler::sUsbDevices;

void UsbEventHandler::handleInserted(
    Window *window,
    const std::string& mountPoint,
    const std::string& devnode)
{
    LOG(LogDebug)
        << "UsbEventHandler: handleInserted START"
        << " mountPoint=" << mountPoint;

    if (!window)
        return;

    std::string playerDataFile = mountPoint + "/ESGI-Game/player_data.json";

    LOG(LogDebug)
        << "UsbEventHandler: playerDataFile="
        << playerDataFile;

    PlayerData player;

    LOG(LogDebug)
        << "UsbEventHandler: loading player_data.json";

    if (!PlayerData::load(
            playerDataFile,
            player))
        return;
    
    LOG(LogDebug)
        << "UsbEventHandler: PlayerData loaded"
        << " id=" << player.getId()
        << " nickname=" << player.getNickname();

    sUsbDevices[devnode] = mountPoint;

    std::string welcomeMessage = "Welcome " + player.getNickname();
    window->displayNotificationMessage(welcomeMessage);

    ProfileManager::getInstance()
        ->addProfile(
            player.getId(),
            player.getNickname(),
            mountPoint);

    auto profiles = ProfileManager::getInstance()->getAvailableProfiles();
    int numControllers = InputManager::getInstance()->getNumConfiguredDevices();

    if (profiles.size() == 1)
    {
        ProfileManager::getInstance()->assignProfileToPlayer(0, profiles[0]);

        for (int i = 1; i < numControllers; i++)
        {
            PlayerProfile guest = {
                "0",
                "Guest",
                "",
                true};

            ProfileManager::getInstance()->assignProfileToPlayer(i, guest);
        }
    } else if(profiles.size() > 1) window->pushGui(new GuiProfileSelector(window));
}

void UsbEventHandler::handleUnplugged(
    Window* window,
    const std::string& devnode 
) {
    if(!window) return;

    auto it = sUsbDevices.find(devnode);
    if(it == sUsbDevices.end()) return;

    const std::string mountPoint = it->second;

    ProfileManager::getInstance()->removeProfile(mountPoint);
    sUsbDevices.erase(it);
    window->displayNotificationMessage("Profile unloaded");
}