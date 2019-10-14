#ifndef TAO_JSON_OBJECT_HPP
#define TAO_JSON_OBJECT_HPP

#include <map>
#include <memory>
#include <functional>
#include <utility>

namespace tao::json::internal {

    template<
        typename ...Types
    >
    class object : public std::map<Types...> {
        using std::map<Types...>::map;
    };
}

#endif