#include "guis/GuiProfileSelector.h"
#include "ProfileManager.h"
#include "InputManager.h"
#include <memory>

GuiProfileSelector::GuiProfileSelector(Window* window) 
    : GuiComponent(window), mMenu(window, "PROFILE MANAGEMENT") {
    
    addChild(&mMenu);
    buildMenu();
    
    setSize(Renderer::getScreenWidth(), Renderer::getScreenHeight());
    mMenu.setPosition((mSize.x() - mMenu.getSize().x()) / 2, Renderer::getScreenHeight() * 0.15f);
}

void GuiProfileSelector::buildMenu() {
    mMenu.clear();
    mSelectors.clear();
    
    int numControllers = InputManager::getInstance()->getNumConfiguredDevices();
    auto profiles = ProfileManager::getInstance()->getAvailableProfiles();

    for (int i = 0; i < numControllers; ++i) {
        InputConfig* inputConfig = InputManager::getInstance()->getInputConfigByDevice(i);
        std::string deviceName = (inputConfig != nullptr) ? inputConfig->getDeviceName() : "Unknown Controller";
        
        std::string playerLabel = "Controller #" + std::to_string(i + 1) + " (" + deviceName + ")";

        auto selector = std::make_shared<OptionListComponent<std::string>>(mWindow, playerLabel, false);
        PlayerProfile currentAssigned = ProfileManager::getInstance()->getPlayerProfile(i);
        
        selector->add("Guest", "", currentAssigned.isGuest);

        for(const auto& p : profiles) {
            bool isSelected = (currentAssigned.mountPoint == p.mountPoint && !currentAssigned.isGuest);
            selector->add(p.nickname, p.mountPoint, isSelected);
        }

        mSelectors.push_back(selector);

        mMenu.addWithLabel(playerLabel, selector);
    }

    mMenu.addButton("CONFIRM", "confirm and close", [this, profiles] {
        for (size_t i = 0; i < mSelectors.size(); ++i) {
            std::vector<std::string> selection = mSelectors[i]->getSelectedObjects();
            
            if (selection.empty() || selection.front() == "") {
                ProfileManager::getInstance()->assignProfileToPlayer(i, {"0", "Guest", "", true});
                LOG(LogDebug) << "Assignment -> Controller #" << (i+1) << ": Guest";
            } else {
                std::string selectedMount = selection.front();
                for (const auto& p : profiles) {
                    if (p.mountPoint == selectedMount) {
                        ProfileManager::getInstance()->assignProfileToPlayer(i, p);
                        LOG(LogDebug) << "Assignment -> Controller #" << (i+1) << ": " << p.nickname << " (" << p.id << ", " << p.mountPoint << ")";
                        break;
                    }
                }
            }
        }
        delete this;
    });
}

bool GuiProfileSelector::input(InputConfig* config, Input input) {
    if (config->isMappedTo(BUTTON_BACK, input) && input.value != 0) {
        delete this;
        return true;
    }
    return GuiComponent::input(config, input);
}

void GuiProfileSelector::update(int deltaTime) {
    GuiComponent::update(deltaTime);
}

void GuiProfileSelector::render(const Transform4x4f& parentTrans) {
    Transform4x4f trans = parentTrans * getTransform();
    renderChildren(trans);
}