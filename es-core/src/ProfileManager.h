#pragma once
#include <string>
#include <vector>
#include <map>

struct PlayerProfile {
    std::string id;
    std::string nickname;
    std::string mountPoint;
    
    bool isGuest;
};

class ProfileManager {
    public:
        static ProfileManager* getInstance();

        void addProfile(const std::string& id, const std::string& nickname, const std::string& mountPoint);

        void removeProfile(const std::string& mountPoint);

        void assignProfileToPlayer(int playerIndex, PlayerProfile profile);

        PlayerProfile getPlayerProfile(int playerIndex);

        std::vector<PlayerProfile> getAvailableProfiles() const;
    
    private:
        ProfileManager();
        std::vector<PlayerProfile> mAvailableProfiles;
        std::map<int, PlayerProfile> mPlayerMapping;
};