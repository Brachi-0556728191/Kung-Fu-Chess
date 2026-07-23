#pragma once


#include "messages_json.h"

namespace net {

template <typename Msg>
nlohmann::json toEnvelope(MessageType type, const Msg& msg) {
    nlohmann::json j = msg;
    j["type"] = type;
    return j;
}

inline MessageType peekType(const nlohmann::json& envelope) {
    return envelope.at("type").get<MessageType>();
}

}  // namespace net
