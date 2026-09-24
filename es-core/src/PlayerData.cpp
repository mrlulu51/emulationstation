#include "PlayerData.h"
#include "Log.h"
#include <fstream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

PlayerData::PlayerData()
    : mId("0"),
      mNickname("Guest")
{
}

PlayerData::PlayerData(
    const std::string &id,
    const std::string &nickname)
    : mId(id),
      mNickname(nickname)
{
}

const std::string &PlayerData::getId() const
{
    return mId;
}

const std::string &PlayerData::getNickname() const
{
    return mNickname;
}

bool PlayerData::load(
    const std::string &filePath,
    PlayerData &player)
{
    std::ifstream file(filePath);
    if (!file.is_open())
        return false;

    try
    {
        json data;
        file >> data;

        player.mId = data.value("id", "0");
        player.mNickname = data.value("nickname", "Guest");

        return true;
    }
    catch (const json::exception &e)
    {
        return false;
    }
}

bool PlayerData::save(
    const std::string &filePath) const
{
    try {
        json data;
        data["id"] = mId;
        data["nickname"] = mNickname;

        std::ofstream file(filePath);

        if(!file.is_open()) return false;

        file << data.dump(4);

        return true;
    } catch (const json::exception& e) {
        return false;
    }
}