#pragma once

#include <string>

class PlayerData
{
    public:
        PlayerData();
        PlayerData(
            const std::string& id,
            const std::string& nickname
        );

        const std::string& getId() const;
        const std::string& getNickname() const;

        static bool load(
            const std::string& filePath,
            PlayerData& player
        );

        bool save(
            const std::string& filePath
        ) const;
    private:
        std::string mId;
        std::string mNickname;
};