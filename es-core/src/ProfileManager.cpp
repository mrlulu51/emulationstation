#include "ProfileManager.h"
#include <algorithm>

ProfileManager* ProfileManager::getInstance() {
    static ProfileManager instance;
    return &instance;
}

ProfileManager::ProfileManager() {}

void ProfileManager::addProfile(const std::string& id, const std::string& nickname, const std::string& mountPoint) {
    for(const auto& p : mAvailableProfiles) {
        if(p.mountPoint == mountPoint) return;
    }
    mAvailableProfiles.push_back({ id, nickname, mountPoint, false });
}

void ProfileManager::removeProfile(const std::string& mountPoint) {
    mAvailableProfiles.erase(
        std::remove_if(mAvailableProfiles.begin(), mAvailableProfiles.end(), [&](const PlayerProfile& p) { return p.mountPoint == mountPoint; }),
        mAvailableProfiles.end()
    );

    for(auto& pair : mPlayerMapping) {
        if(pair.second.mountPoint == mountPoint) {
            pair.second = { "0", "Guest", "", true };
        }
    }
}

void ProfileManager::assignProfileToPlayer(int playerIndex, PlayerProfile profile) {
    mPlayerMapping[playerIndex] = profile;
}

PlayerProfile ProfileManager::getPlayerProfile(int playerIndex) {
    auto it = mPlayerMapping.find(playerIndex);
    if(it != mPlayerMapping.end()) {
        return it->second;
    }

    return {"0", "Guest", "", true};
}

std::vector<PlayerProfile> ProfileManager::getAvailableProfiles() const {
    return mAvailableProfiles;
}