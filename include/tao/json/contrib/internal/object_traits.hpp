// Copyright (c) 2018-2019 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/json/

#ifndef TAO_JSON_CONTRIB_INTERNAL_OBJECT_TRAITS_HPP
#define TAO_JSON_CONTRIB_INTERNAL_OBJECT_TRAITS_HPP

#include <algorithm>
#include <string>

#include "../../forward.hpp"
#include "../../type.hpp"

#include "../../events/produce.hpp"

namespace tao::json::internal
{
   template< typename T >
   struct object_multi_traits
   {
      template< template< typename... > class Traits >
      [[nodiscard]] static bool is_nothing( const T& o )
      {
         return o.empty();
      }

      template< template< typename... > class Traits >
      static void assign( basic_value< Traits >& v, const T& o ) = delete;

      template< template< typename... > class Traits, typename Consumer >
      static void produce( Consumer& c, const T& o )
      {
         c.begin_object( o.size() );
         for( const auto& i : o ) {
            c.key( i.first );
            json::events::produce< Traits >( c, i.second );
            c.member();
         }
         c.end_object( o.size() );
      }

      template< template< typename... > class Traits >
      [[nodiscard]] static bool equal( const basic_value< Traits >& lhs, const T& rhs ) noexcept
      {
         static const auto eq = []( const typename T::value_type& r, const std::pair< const std::string, basic_value< Traits > >& l ) {
            return ( l.first == r.first ) && ( l.second == r.second );
         };
         const auto& p = lhs.skip_value_ptr();
         return p.is_object() && ( p.get_object().size() == rhs.size() ) && std::equal( rhs.begin(), rhs.end(), p.get_object().begin(), eq );
      }

      struct pair_less
      {
         template< typename L, typename R >
         [[nodiscard]] bool operator()( const L& l, const R& r ) const noexcept
         {
            return ( l.first < r.first ) || ( ( l.first == r.first ) && ( l.second < r.second ) );
         }
      };

      template< template< typename... > class Traits >
      [[nodiscard]] static bool less_than( const basic_value< Traits >& lhs, const T& rhs ) noexcept
      {
         const auto& p = lhs.skip_value_ptr();
         return p.is_object() ? std::lexicographical_compare( p.get_object().begin(), p.get_object().end(), rhs.begin(), rhs.end(), pair_less() ) : ( p.type() < type::OBJECT );
      }

      template< template< typename... > class Traits >
      [[nodiscard]] static bool greater_than( const basic_value< Traits >& lhs, const T& rhs ) noexcept
      {
         const auto& p = lhs.skip_value_ptr();
         return p.is_object() ? std::lexicographical_compare( rhs.begin(), rhs.end(), p.get_object().begin(), p.get_object().end(), pair_less() ) : ( p.type() > type::OBJECT );
      }
   };

   template< typename T >
   struct object_traits
      : object_multi_traits< T >
   {
      template< template< typename... > class Traits >
      static void assign( basic_value< Traits >& v, const T& o )
      {
         v.prepare_object();
         for( const auto& i : o ) {
            v.try_emplace( i.first, i.second );
         }
      }
   };

}  // namespace tao::json::internal

#endif
