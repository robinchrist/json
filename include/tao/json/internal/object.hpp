#ifndef TAO_JSON_OBJECT_HPP
#define TAO_JSON_OBJECT_HPP

#include <map>
#include <optional>
#include <functional>

namespace tao::json::internal {

    template<typename ...Types>
    class object : public std::map<Types...> {
    public:
        using wrapped_map_t = std::map<Types...>;

        //We want to inherit the constructors, because this is just a thin wrapper providing additional convenience functions
        using std::map<Types...>::map;

        std::optional<std::reference_wrapper<typename wrapped_map_t::mapped_type>> try_at(const typename wrapped_map_t::key_type& key) {
            auto mapIt = wrapped_map_t::find(key);
            if(mapIt == wrapped_map_t::end()) {
                return std::nullopt;
            }

            return (*mapIt).second;
        }

        std::optional<std::reference_wrapper<const typename wrapped_map_t::mapped_type>> try_at(const typename wrapped_map_t::key_type& key) const {
            auto mapIt = wrapped_map_t::find(key);
            if(mapIt == wrapped_map_t::end()) {
                return std::nullopt;
            }

            return (*mapIt).second;
        }

        //Nicer "at" equivalents (for when a key is required)
        typename wrapped_map_t::mapped_type& at_key(const typename wrapped_map_t::key_type& key) {
            auto mapIt = wrapped_map_t::find(key);
            if(mapIt == wrapped_map_t::end()) {
                throw std::out_of_range(internal::format("could not find key '", key, '\''));
            }

            return (*mapIt).second;
        }

        const typename wrapped_map_t::mapped_type& at_key(const typename wrapped_map_t::key_type& key) const {
            auto mapIt = wrapped_map_t::find(key);
            if(mapIt == wrapped_map_t::end()) {
                throw std::out_of_range(internal::format("could not find key '", key, '\''));
            }

            return (*mapIt).second;
        }

    };
}

#endif
