// Copyright (c) 2015-2019 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/json/

#ifndef TAO_JSON_INTERNAL_VALUE_UNION_HPP
#define TAO_JSON_INTERNAL_VALUE_UNION_HPP

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "../binary_view.hpp"
#include "../forward.hpp"

namespace tao::json::internal
{
   struct opaque_ptr_t
   {
      const void* data;
      producer_t producer;
   };

   template< typename T >
   union value_union
   {
      value_union() noexcept
      {}

      value_union( const value_union& ) = delete;
      value_union( value_union&& ) = delete;

      ~value_union() noexcept
      {}

      void operator=( const value_union& ) = delete;
      void operator=( value_union&& ) = delete;

      bool b;

      std::int64_t i;
      std::uint64_t u;
      double d;

      std::string s;
      std::string_view sv;

      std::vector< std::byte > x;
      tao::binary_view xv;

      typename T::array_t a;
      typename T::object_t o;

      const T* p;
      opaque_ptr_t q;
   };

}  // namespace tao::json::internal

#endif
