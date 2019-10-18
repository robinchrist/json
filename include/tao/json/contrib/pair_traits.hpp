// Copyright (c) 2018-2019 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/json/

#ifndef TAO_JSON_CONTRIB_PAIR_TRAITS_HPP
#define TAO_JSON_CONTRIB_PAIR_TRAITS_HPP

#include <utility>

#include "../binding.hpp"

namespace tao::json
{
   template< typename U, typename V >
   struct pair_traits
      : binding::array< TAO_JSON_BIND_ELEMENT( &std::pair< U, V >::first ),
                        TAO_JSON_BIND_ELEMENT( &std::pair< U, V >::second ) >
   {};

   template< typename U, typename V>
   struct fixed_pair_traits {

        template<template<typename...> class Traits>
        static void assign(tao::json::basic_value<Traits>& v, const std::pair<U, V>& pair) {
            v = tao::json::basic_value<Traits>::array({
                pair.first,
                pair.second
            });
        }

        template<template<typename...> class Traits>
        static std::pair<U, V> as(const tao::json::basic_value<Traits>& v ) {
            const auto& array = v.get_array();

            if(array.size() != 2) {
                throw std::runtime_error(json::internal::format("array size mismatch - expected 2, got ", array.size()));
            }

            return std::pair<U, V> {
                array.at(0).template as<U>(),
                array.at(1).template as<V>()
            };
        }
   };

}  // namespace tao::json

#endif
