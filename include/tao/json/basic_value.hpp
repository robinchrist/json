// Copyright (c) 2017-2019 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/json/

#ifndef TAO_JSON_BASIC_VALUE_HPP
#define TAO_JSON_BASIC_VALUE_HPP

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "events/virtual_base.hpp"

#include "internal/escape.hpp"
#include "internal/format.hpp"
#include "internal/get_by_enum.hpp"
#include "internal/identity.hpp"
#include "internal/object.hpp"
#include "internal/pair.hpp"
#include "internal/single.hpp"
#include "internal/type_traits.hpp"
#include "internal/value_union.hpp"

#include "binary_view.hpp"
#include "message_extension.hpp"
#include "pointer.hpp"
#include "type.hpp"

namespace tao::json
{
   template< template< typename... > class Traits >
   class basic_value
      : public Traits< void >::template public_base< basic_value< Traits > >
   {
   public:
      using public_base_t = typename Traits< void >::template public_base< basic_value< Traits > >;

      static_assert( !std::is_default_constructible_v< public_base_t > || std::is_nothrow_default_constructible_v< public_base_t > );
      static_assert( std::is_nothrow_move_constructible_v< public_base_t > );
      static_assert( std::is_nothrow_move_assignable_v< public_base_t > );

      using array_t = std::vector< basic_value >;
      //using object_t = std::map< std::string, basic_value, std::less<> >;
      using object_t = internal::object< std::string, basic_value, std::less<> >;

      basic_value() noexcept = default;

      basic_value( const basic_value& r )
         : public_base_t( static_cast< const public_base_t& >( r ) ),
           m_type( json::type::DESTROYED )
      {
         embed( r );
         m_type = r.m_type;
      }

      basic_value( basic_value&& r ) noexcept
         : public_base_t( static_cast< public_base_t&& >( r ) ),
           m_type( r.m_type )
      {
         seize( std::move( r ) );
      }

      basic_value( const uninitialized_t /*unused*/, public_base_t b ) noexcept
         : public_base_t( std::move( b ) )
      {
      }

      template< typename T,
                typename = std::enable_if_t< internal::enable_implicit_constructor< Traits, std::decay_t< T > > >,
                typename = decltype( Traits< std::decay_t< T > >::assign( std::declval< basic_value& >(), std::declval< T&& >() ) ) >
      basic_value( T&& v, public_base_t b = public_base_t() ) noexcept( noexcept( Traits< std::decay_t< T > >::assign( std::declval< basic_value& >(), std::forward< T >( v ) ) ) )
         : public_base_t( std::move( b ) )
      {
         if constexpr( noexcept( Traits< std::decay_t< T > >::assign( std::declval< basic_value& >(), std::forward< T >( v ) ) ) ) {
            using D = std::decay_t< T >;
            Traits< D >::assign( *this, std::forward< T >( v ) );
         }
         else {
            try {
               using D = std::decay_t< T >;
               Traits< D >::assign( *this, std::forward< T >( v ) );
            }
            catch( ... ) {
               unsafe_discard();
#ifndef NDEBUG
               static_cast< volatile json::type& >( m_type ) = json::type::DESTROYED;
#endif
               throw;
            }
         }
      }

      template< typename T,
                typename = std::enable_if_t< !internal::enable_implicit_constructor< Traits, std::decay_t< T > > >,
                typename = decltype( Traits< std::decay_t< T > >::assign( std::declval< basic_value& >(), std::declval< T&& >() ) ),
                int = 0 >
      explicit basic_value( T&& v, public_base_t b = public_base_t() ) noexcept( noexcept( Traits< std::decay_t< T > >::assign( std::declval< basic_value& >(), std::forward< T >( v ) ) ) )
         : public_base_t( std::move( b ) )
      {
         if constexpr( noexcept( Traits< std::decay_t< T > >::assign( std::declval< basic_value& >(), std::forward< T >( v ) ) ) ) {
            using D = std::decay_t< T >;
            Traits< D >::assign( *this, std::forward< T >( v ) );
         }
         else {
            try {
               using D = std::decay_t< T >;
               Traits< D >::assign( *this, std::forward< T >( v ) );
            }
            catch( ... ) {
               unsafe_discard();
#ifndef NDEBUG
               static_cast< volatile json::type& >( m_type ) = json::type::DESTROYED;
#endif
               throw;
            }
         }
      }

      basic_value( std::initializer_list< internal::pair< Traits > >&& l, public_base_t b = public_base_t() )
         : public_base_t( std::move( b ) )
      {
         try {
            unsafe_assign( std::move( l ) );
         }
         catch( ... ) {
            unsafe_discard();
#ifndef NDEBUG
            static_cast< volatile json::type& >( m_type ) = json::type::DESTROYED;
#endif
            throw;
         }
      }

      basic_value( const std::initializer_list< internal::pair< Traits > >& l, public_base_t b = public_base_t() )
         : public_base_t( std::move( b ) )
      {
         try {
            unsafe_assign( l );
         }
         catch( ... ) {
            unsafe_discard();
#ifndef NDEBUG
            static_cast< volatile json::type& >( m_type ) = json::type::DESTROYED;
#endif
            throw;
         }
      }

      basic_value( std::initializer_list< internal::pair< Traits > >& l, public_base_t b = public_base_t() )
         : basic_value( static_cast< const std::initializer_list< internal::pair< Traits > >& >( l ), std::move( b ) )
      {}

      ~basic_value() noexcept
      {
         unsafe_discard();
#ifndef NDEBUG
         static_cast< volatile json::type& >( m_type ) = json::type::DESTROYED;
#endif
      }

      [[nodiscard]] static basic_value array( std::initializer_list< internal::single< Traits > >&& l, public_base_t b = public_base_t() )
      {
         basic_value v( uninitialized, std::move( b ) );
         v.append( std::move( l ) );
         return v;
      }

      [[nodiscard]] static basic_value array( const std::initializer_list< internal::single< Traits > >& l, public_base_t b = public_base_t() )
      {
         basic_value v( uninitialized, std::move( b ) );
         v.append( l );
         return v;
      }

      [[nodiscard]] static basic_value object( std::initializer_list< internal::pair< Traits > >&& l, public_base_t b = public_base_t() )
      {
         basic_value v( uninitialized, std::move( b ) );
         v.insert( std::move( l ) );
         return v;
      }

      [[nodiscard]] static basic_value object( const std::initializer_list< internal::pair< Traits > >& l, public_base_t b = public_base_t() )
      {
         basic_value v( uninitialized, std::move( b ) );
         v.insert( l );
         return v;
      }

      basic_value& operator=( basic_value v ) noexcept
      {
         unsafe_discard();
         m_type = v.m_type;
         seize( std::move( v ) );
         public_base_t::operator=( static_cast< public_base_t&& >( v ) );
         return *this;
      }

      void swap( basic_value& r ) noexcept
      {
         basic_value t( std::move( r ) );
         r = std::move( *this );
         ( *this ) = ( std::move( t ) );
      }

      [[nodiscard]] public_base_t& public_base() noexcept
      {
         return static_cast< public_base_t& >( *this );
      }

      [[nodiscard]] const public_base_t& public_base() const noexcept
      {
         return static_cast< const public_base_t& >( *this );
      }

      [[nodiscard]] json::type type() const noexcept
      {
         return m_type;
      }

      [[nodiscard]] explicit operator bool() const noexcept
      {
         assert( m_type != json::type::DISCARDED );
         assert( m_type != json::type::DESTROYED );
         return m_type != json::type::UNINITIALIZED;
      }

      [[nodiscard]] bool is_null() const noexcept
      {
         return m_type == json::type::NULL_;
      }

      [[nodiscard]] bool is_boolean() const noexcept
      {
         return m_type == json::type::BOOLEAN;
      }

      [[nodiscard]] bool is_signed() const noexcept
      {
         return m_type == json::type::SIGNED;
      }

      [[nodiscard]] bool is_unsigned() const noexcept
      {
         return m_type == json::type::UNSIGNED;
      }

      [[nodiscard]] bool is_integer() const noexcept
      {
         return is_signed() || is_unsigned();
      }

      [[nodiscard]] bool is_double() const noexcept
      {
         return m_type == json::type::DOUBLE;
      }

      [[nodiscard]] bool is_number() const noexcept
      {
         return is_integer() || is_double();
      }

      [[nodiscard]] bool is_string() const noexcept
      {
         return m_type == json::type::STRING;
      }

      [[nodiscard]] bool is_string_view() const noexcept
      {
         return m_type == json::type::STRING_VIEW;
      }

      [[nodiscard]] bool is_string_type() const noexcept
      {
         return is_string() || is_string_view();
      }

      [[nodiscard]] bool is_binary() const noexcept
      {
         return m_type == json::type::BINARY;
      }

      [[nodiscard]] bool is_binary_view() const noexcept
      {
         return m_type == json::type::BINARY_VIEW;
      }

      [[nodiscard]] bool is_binary_type() const noexcept
      {
         return is_binary() || is_binary_view();
      }

      [[nodiscard]] bool is_array() const noexcept
      {
         return m_type == json::type::ARRAY;
      }

      [[nodiscard]] bool is_object() const noexcept
      {
         return m_type == json::type::OBJECT;
      }

      [[nodiscard]] bool is_value_ptr() const noexcept
      {
         return m_type == json::type::VALUE_PTR;
      }

      [[nodiscard]] bool is_opaque_ptr() const noexcept
      {
         return m_type == json::type::OPAQUE_PTR;
      }

      // [[nodiscard]] bool is_destroyed() const noexcept
      // {
      //    return m_type == json::type::DESTROYED;
      // }

      [[nodiscard]] bool is_discarded() const noexcept
      {
         return m_type == json::type::DISCARDED;
      }

      [[nodiscard]] bool is_uninitialized() const noexcept
      {
         return m_type == json::type::UNINITIALIZED;
      }

      // The unsafe_get_*() accessor functions MUST NOT be
      // called when the type of the value is not the one
      // corresponding to the type of the accessor!

      [[nodiscard]] bool unsafe_get_boolean() const noexcept
      {
         return m_union.b;
      }

      [[nodiscard]] std::int64_t unsafe_get_signed() const noexcept
      {
         return m_union.i;
      }

      [[nodiscard]] std::uint64_t unsafe_get_unsigned() const noexcept
      {
         return m_union.u;
      }

      [[nodiscard]] double unsafe_get_double() const noexcept
      {
         return m_union.d;
      }

      [[nodiscard]] std::string& unsafe_get_string() noexcept
      {
         return m_union.s;
      }

      [[nodiscard]] const std::string& unsafe_get_string() const noexcept
      {
         return m_union.s;
      }

      [[nodiscard]] std::string_view unsafe_get_string_view() const noexcept
      {
         return m_union.sv;
      }

      [[nodiscard]] std::string_view unsafe_get_string_type() const noexcept
      {
         return ( m_type == json::type::STRING ) ? m_union.s : m_union.sv;
      }

      [[nodiscard]] binary& unsafe_get_binary() noexcept
      {
         return m_union.x;
      }

      [[nodiscard]] const binary& unsafe_get_binary() const noexcept
      {
         return m_union.x;
      }

      [[nodiscard]] tao::binary_view unsafe_get_binary_view() const noexcept
      {
         return m_union.xv;
      }

      [[nodiscard]] tao::binary_view unsafe_get_binary_type() const noexcept
      {
         return ( m_type == json::type::BINARY ) ? m_union.x : m_union.xv;
      }

      [[nodiscard]] array_t& unsafe_get_array() noexcept
      {
         return m_union.a;
      }

      [[nodiscard]] const array_t& unsafe_get_array() const noexcept
      {
         return m_union.a;
      }

      [[nodiscard]] object_t& unsafe_get_object() noexcept
      {
         return m_union.o;
      }

      [[nodiscard]] const object_t& unsafe_get_object() const noexcept
      {
         return m_union.o;
      }

      [[nodiscard]] const basic_value* unsafe_get_value_ptr() const noexcept
      {
         return m_union.p;
      }

      [[nodiscard]] internal::opaque_ptr_t unsafe_get_opaque_ptr() const noexcept
      {
         return m_union.q;
      }

      template< json::type E >
      [[nodiscard]] decltype( internal::get_by_enum< E >::get( std::declval< internal::value_union< basic_value >& >() ) ) unsafe_get() noexcept
      {
         return internal::get_by_enum< E >::get( m_union );
      }

      template< json::type E >
      [[nodiscard]] decltype( internal::get_by_enum< E >::get( std::declval< const internal::value_union< basic_value >& >() ) ) unsafe_get() const noexcept
      {
         return internal::get_by_enum< E >::get( m_union );
      }

      [[nodiscard]] bool get_boolean() const
      {
         validate_json_type( json::type::BOOLEAN );
         return unsafe_get_boolean();
      }

      [[nodiscard]] std::int64_t get_signed() const
      {
         validate_json_type( json::type::SIGNED );
         return unsafe_get_signed();
      }

      [[nodiscard]] std::uint64_t get_unsigned() const
      {
         validate_json_type( json::type::UNSIGNED );
         return unsafe_get_unsigned();
      }

      [[nodiscard]] double get_double() const
      {
         validate_json_type( json::type::DOUBLE );
         return unsafe_get_double();
      }

      [[nodiscard]] std::string& get_string()
      {
         validate_json_type( json::type::STRING );
         return unsafe_get_string();
      }

      [[nodiscard]] const std::string& get_string() const
      {
         validate_json_type( json::type::STRING );
         return unsafe_get_string();
      }

      [[nodiscard]] std::string_view get_string_view() const
      {
         validate_json_type( json::type::STRING_VIEW );
         return unsafe_get_string_view();
      }

      [[nodiscard]] std::string_view get_string_type() const noexcept
      {
         return ( m_type == json::type::STRING_VIEW ) ? m_union.sv : get_string();
      }

      [[nodiscard]] binary& get_binary()
      {
         validate_json_type( json::type::BINARY );
         return unsafe_get_binary();
      }

      [[nodiscard]] const binary& get_binary() const
      {
         validate_json_type( json::type::BINARY );
         return unsafe_get_binary();
      }

      [[nodiscard]] tao::binary_view get_binary_view() const
      {
         validate_json_type( json::type::BINARY_VIEW );
         return unsafe_get_binary_view();
      }

      [[nodiscard]] tao::binary_view get_binary_type() const noexcept
      {
         return ( m_type == json::type::BINARY_VIEW ) ? m_union.xv : get_binary();
      }

      [[nodiscard]] array_t& get_array()
      {
         validate_json_type( json::type::ARRAY );
         return unsafe_get_array();
      }

      [[nodiscard]] const array_t& get_array() const
      {
         validate_json_type( json::type::ARRAY );
         return unsafe_get_array();
      }

      [[nodiscard]] object_t& get_object()
      {
         validate_json_type( json::type::OBJECT );
         return unsafe_get_object();
      }

      [[nodiscard]] const object_t& get_object() const
      {
         validate_json_type( json::type::OBJECT );
         return unsafe_get_object();
      }

      [[nodiscard]] const basic_value* get_value_ptr() const
      {
         validate_json_type( json::type::VALUE_PTR );
         return unsafe_get_value_ptr();
      }

      [[nodiscard]] internal::opaque_ptr_t get_opaque_ptr() const noexcept
      {
         validate_json_type( json::type::OPAQUE_PTR );
         return unsafe_get_opaque_ptr();
      }

      template< json::type E >
      [[nodiscard]] decltype( internal::get_by_enum< E >::get( std::declval< internal::value_union< basic_value >& >() ) ) get()
      {
         validate_json_type( E );
         return internal::get_by_enum< E >::get( m_union );
      }

      template< json::type E >
      [[nodiscard]] decltype( internal::get_by_enum< E >::get( std::declval< const internal::value_union< basic_value >& >() ) ) get() const
      {
         validate_json_type( E );
         return internal::get_by_enum< E >::get( m_union );
      }

   private:
      void throw_duplicate_key_exception( const std::string_view k ) const
      {
         throw std::runtime_error( internal::format( "duplicate JSON object key \"", internal::escape( k ), '"', json::message_extension( *this ) ) );
      }

      void throw_index_out_of_bound_exception( const std::size_t i ) const
      {
         throw std::out_of_range( internal::format( "JSON array index '", i, "' out of bound '", m_union.a.size(), '\'', json::message_extension( *this ) ) );
      }

      void throw_key_not_found_exception( const std::string_view k ) const
      {
         throw std::out_of_range( internal::format( "JSON object key \"", internal::escape( k ), "\" not found", json::message_extension( *this ) ) );
      }

   public:
      void unsafe_assign_null() noexcept
      {
         m_type = json::type::NULL_;
      }

      void unsafe_assign_boolean( const bool b ) noexcept
      {
         m_union.b = b;
         m_type = json::type::BOOLEAN;
      }

      void unsafe_assign_signed( const std::int64_t i ) noexcept
      {
         m_union.i = i;
         m_type = json::type::SIGNED;
      }

      void unsafe_assign_unsigned( const std::uint64_t u ) noexcept
      {
         m_union.u = u;
         m_type = json::type::UNSIGNED;
      }

      void unsafe_assign_double( const double d ) noexcept
      {
         m_union.d = d;
         m_type = json::type::DOUBLE;
      }

      template< typename... Ts >
      void unsafe_emplace_string( Ts&&... ts ) noexcept( noexcept( std::string( std::forward< Ts >( ts )... ) ) )
      {
         ::new( &m_union.s ) std::string( std::forward< Ts >( ts )... );
         m_type = json::type::STRING;
      }

      void unsafe_assign_string( const std::string& s )
      {
         unsafe_emplace_string( s );
      }

      void unsafe_assign_string( std::string&& s ) noexcept
      {
         unsafe_emplace_string( std::move( s ) );
      }

      void unsafe_assign_string_view( const std::string_view sv ) noexcept
      {
         m_union.sv = sv;
         m_type = json::type::STRING_VIEW;
      }

      template< typename... Ts >
      void unsafe_emplace_binary( Ts&&... ts ) noexcept( noexcept( binary( std::forward< Ts >( ts )... ) ) )
      {
         new( &m_union.x ) binary( std::forward< Ts >( ts )... );
         m_type = json::type::BINARY;
      }

      void unsafe_assign_binary( const binary& x )
      {
         unsafe_emplace_binary( x );
      }

      void unsafe_assign_binary( binary&& x ) noexcept
      {
         unsafe_emplace_binary( std::move( x ) );
      }

      void unsafe_assign_binary_view( const tao::binary_view xv ) noexcept
      {
         m_union.xv = xv;
         m_type = json::type::BINARY_VIEW;
      }

      template< typename... Ts >
      void unsafe_emplace_array( Ts&&... ts ) noexcept( noexcept( array_t( std::forward< Ts >( ts )... ) ) )
      {
         new( &m_union.a ) array_t( std::forward< Ts >( ts )... );
         m_type = json::type::ARRAY;
      }

      void unsafe_assign_array( const array_t& a )
      {
         unsafe_emplace_array( a );
      }

      void unsafe_assign_array( array_t&& a ) noexcept
      {
         unsafe_emplace_array( std::move( a ) );
      }

      void unsafe_push_back( const basic_value& v )
      {
         m_union.a.push_back( v );
      }

      void unsafe_push_back( basic_value&& v )
      {
         m_union.a.push_back( std::move( v ) );
      }

      template< typename... Ts >
      basic_value& unsafe_emplace_back( Ts&&... ts )
      {
         return m_union.a.emplace_back( std::forward< Ts >( ts )... );
      }

      template< typename... Ts >
      void unsafe_emplace_object( Ts&&... ts ) noexcept( noexcept( object_t( std::forward< Ts >( ts )... ) ) )
      {
         new( &m_union.o ) object_t( std::forward< Ts >( ts )... );
         m_type = json::type::OBJECT;
      }

      void unsafe_assign_object( const object_t& o )
      {
         unsafe_emplace_object( o );
      }

      void unsafe_assign_object( object_t&& o ) noexcept
      {
         unsafe_emplace_object( std::move( o ) );
      }

      // template< typename... Ts >
      // [[deprecated( "please use unsafe_try_emplace()" )]] auto unsafe_emplace( Ts&&... ts )
      // {
      //    auto r = m_union.o.emplace( std::forward< Ts >( ts )... );
      //    if( !r.second ) {
      //       throw_duplicate_key_exception( r.first->first );
      //    }
      //    return r;
      // }

      template< typename... Ts >
      auto unsafe_try_emplace( Ts&&... ts )
      {
         auto r = m_union.o.try_emplace( std::forward< Ts >( ts )... );
         if( !r.second ) {
            throw_duplicate_key_exception( r.first->first );
         }
         return r;
      }

      auto unsafe_insert( typename object_t::value_type&& t )
      {
         return m_union.o.emplace( std::move( t ) );
      }

      auto unsafe_insert( const typename object_t::value_type& t )
      {
         return m_union.o.emplace( t );
      }

      void unsafe_assign_value_ptr( const basic_value* p ) noexcept
      {
         assert( p );
         m_union.p = p;
         m_type = json::type::VALUE_PTR;
      }

      template< typename T >
      void unsafe_assign_opaque_ptr( const T* data ) noexcept
      {
         unsafe_assign_opaque_ptr( data, &basic_value::producer_wrapper< T > );
      }

      template< typename T >
      void unsafe_assign_opaque_ptr( const T* data, const producer_t producer ) noexcept
      {
         assert( data );
         assert( producer );
         m_union.q.data = data;
         m_union.q.producer = producer;
         m_type = json::type::OPAQUE_PTR;
      }

      template< typename T >
      void unsafe_assign( T&& v ) noexcept( noexcept( Traits< std::decay_t< T > >::assign( std::declval< basic_value& >(), std::forward< T >( v ) ) ) )
      {
         using D = std::decay_t< T >;
         Traits< D >::assign( *this, std::forward< T >( v ) );
      }

      template< typename T >
      void unsafe_assign( T&& v, public_base_t b ) noexcept( noexcept( Traits< std::decay_t< T > >::assign( std::declval< basic_value& >(), std::forward< T >( v ) ) ) )
      {
         using D = std::decay_t< T >;
         Traits< D >::assign( *this, std::forward< T >( v ) );
         public_base() = std::move( b );
      }

      void unsafe_assign( std::initializer_list< internal::pair< Traits > >&& l )
      {
         unsafe_emplace_object();
         for( auto& e : l ) {
            unsafe_try_emplace( std::move( e.key ), std::move( e.value ) );
         }
      }

      void unsafe_assign( const std::initializer_list< internal::pair< Traits > >& l )
      {
         unsafe_emplace_object();
         for( const auto& e : l ) {
            unsafe_try_emplace( e.key, e.value );
         }
      }

      void unsafe_assign( std::initializer_list< internal::pair< Traits > >& l )
      {
         unsafe_assign( static_cast< const std::initializer_list< internal::pair< Traits > >& >( l ) );
      }

      void assign_null() noexcept
      {
         unsafe_discard();
         unsafe_assign_null();
      }

      void assign_boolean( const bool b ) noexcept
      {
         unsafe_discard();
         unsafe_assign_boolean( b );
      }

      void assign_signed( const std::int64_t i ) noexcept
      {
         unsafe_discard();
         unsafe_assign_signed( i );
      }

      void assign_unsigned( const std::uint64_t u ) noexcept
      {
         unsafe_discard();
         unsafe_assign_unsigned( u );
      }

      void assign_double( const double d ) noexcept
      {
         unsafe_discard();
         unsafe_assign_double( d );
      }

      template< typename... Ts >
      void emplace_string( Ts&&... ts ) noexcept( noexcept( unsafe_emplace_string( std::forward< Ts >( ts )... ) ) )
      {
         discard();
         unsafe_emplace_string( std::forward< Ts >( ts )... );
      }

      void assign_string( const std::string& s )
      {
         discard();
         unsafe_assign_string( s );
      }

      void assign_string( std::string&& s ) noexcept
      {
         unsafe_discard();
         unsafe_assign_string( std::move( s ) );
      }

      void assign_string_view( const std::string_view sv ) noexcept
      {
         unsafe_discard();
         unsafe_assign_string_view( sv );
      }

      template< typename... Ts >
      void emplace_binary( Ts&&... ts ) noexcept( noexcept( unsafe_emplace_binary( std::forward< Ts >( ts )... ) ) )
      {
         discard();
         unsafe_emplace_binary( std::forward< Ts >( ts )... );
      }

      void assign_binary( const binary& v )
      {
         discard();
         unsafe_assign_binary( v );
      }

      void assign_binary( binary&& v ) noexcept
      {
         unsafe_discard();
         unsafe_assign_binary( std::move( v ) );
      }

      void assign_binary_view( const tao::binary_view xv ) noexcept
      {
         unsafe_discard();
         unsafe_assign_binary_view( xv );
      }

      template< typename... Ts >
      void emplace_array( Ts&&... ts ) noexcept( noexcept( unsafe_emplace_array( std::forward< Ts >( ts )... ) ) )
      {
         discard();
         unsafe_emplace_array( std::forward< Ts >( ts )... );
      }

      void assign_array( const array_t& v )
      {
         discard();
         unsafe_assign_array( v );
      }

      void assign_array( array_t&& v ) noexcept
      {
         unsafe_discard();
         unsafe_assign_array( std::move( v ) );
      }

      void prepare_array()
      {
         switch( m_type ) {
            case json::type::UNINITIALIZED:
            case json::type::DISCARDED:
               unsafe_emplace_array();
            case json::type::ARRAY:
               break;
            default:
               throw std::logic_error( internal::format( "invalid json type '", m_type, "' for prepare_array()", json::message_extension( *this ) ) );
         }
      }

      void push_back( const basic_value& v )
      {
         prepare_array();
         unsafe_push_back( v );
      }

      void push_back( basic_value&& v )
      {
         prepare_array();
         unsafe_push_back( std::move( v ) );
      }

      template< typename... Ts >
      basic_value& emplace_back( Ts&&... ts )
      {
         prepare_array();
         return unsafe_emplace_back( std::forward< Ts >( ts )... );
      }

      void append( std::initializer_list< internal::single< Traits > >&& l )
      {
         prepare_array();
         auto& v = unsafe_get_array();
         v.reserve( v.size() + l.size() );
         for( auto& e : l ) {
            unsafe_push_back( std::move( e.value ) );
         }
      }

      void append( const std::initializer_list< internal::single< Traits > >& l )
      {
         prepare_array();
         auto& v = unsafe_get_array();
         v.reserve( v.size() + l.size() );
         for( const auto& e : l ) {
            unsafe_push_back( e.value );
         }
      }

      template< typename... Ts >
      void emplace_object( Ts&&... ts ) noexcept( noexcept( unsafe_emplace_object( std::forward< Ts >( ts )... ) ) )
      {
         discard();
         unsafe_emplace_object( std::forward< Ts >( ts )... );
      }

      void assign_object( const object_t& o )
      {
         discard();
         unsafe_assign_object( o );
      }

      void assign_object( object_t&& o ) noexcept
      {
         unsafe_discard();
         unsafe_assign_object( std::move( o ) );
      }

      void prepare_object()
      {
         switch( m_type ) {
            case json::type::UNINITIALIZED:
            case json::type::DISCARDED:
               unsafe_emplace_object();
            case json::type::OBJECT:
               break;
            default:
               throw std::logic_error( internal::format( "invalid json type '", m_type, "' for prepare_object()", json::message_extension( *this ) ) );
         }
      }

      // template< typename... Ts >
      // [[deprecated( "please use try_emplace()" )]] auto emplace( Ts&&... ts )
      // {
      //    prepare_object();
      //    return unsafe_emplace( std::forward< Ts >( ts )... );
      // }

      template< typename... Ts >
      auto try_emplace( Ts&&... ts )
      {
         prepare_object();
         return unsafe_try_emplace( std::forward< Ts >( ts )... );
      }

      auto insert( typename object_t::value_type&& t )
      {
         prepare_object();
         return unsafe_insert( std::move( t ) );
      }

      auto insert( const typename object_t::value_type& t )
      {
         prepare_object();
         return unsafe_insert( t );
      }

      void insert( std::initializer_list< internal::pair< Traits > >&& l )
      {
         prepare_object();
         for( auto& e : l ) {
            unsafe_try_emplace( std::move( e.key ), std::move( e.value ) );
         }
      }

      void insert( const std::initializer_list< internal::pair< Traits > >& l )
      {
         prepare_object();
         for( const auto& e : l ) {
            unsafe_try_emplace( e.key, e.value );
         }
      }

      void assign_value_ptr( const basic_value* p ) noexcept
      {
         unsafe_discard();
         unsafe_assign_value_ptr( p );
      }

      template< typename T >
      void assign_opaque_ptr( const T* data ) noexcept
      {
         assign_opaque_ptr( data, &basic_value::producer_wrapper< T > );
      }

      template< typename T >
      void assign_opaque_ptr( const T* data, const producer_t producer ) noexcept
      {
         unsafe_discard();
         unsafe_assign_opaque_ptr( data, producer );
      }

      template< typename T >
      void assign( T&& v ) noexcept( noexcept( std::declval< basic_value& >().template unsafe_assign< T >( std::forward< T >( v ) ) ) )
      {
         unsafe_discard();
         unsafe_assign( std::forward< T >( v ) );
      }

      template< typename T >
      void assign( T&& v, public_base_t b ) noexcept( noexcept( std::declval< basic_value& >().template unsafe_assign< T >( std::forward< T >( v ) ) ) )
      {
         unsafe_discard();
         unsafe_assign( std::forward< T >( v ), std::move( b ) );
      }

      void assign( std::initializer_list< internal::pair< Traits > >&& l )
      {
         unsafe_discard();
         unsafe_assign( std::move( l ) );
      }

      void assign( const std::initializer_list< internal::pair< Traits > >& l )
      {
         unsafe_discard();
         unsafe_assign( l );
      }

      [[nodiscard]] const basic_value& skip_value_ptr() const noexcept
      {
         const basic_value* p = this;
         while( p->is_value_ptr() ) {
            p = p->unsafe_get_value_ptr();
            assert( p );
         }
         return *p;
      }

      template< typename Index >
      [[nodiscard]] std::enable_if_t< std::is_convertible_v< Index, std::size_t >, basic_value* > unsafe_find( Index&& index ) noexcept
      {
         return ( static_cast< std::size_t >( index ) < m_union.a.size() ) ? ( m_union.a.data() + static_cast< std::size_t >( index ) ) : nullptr;
      }

      template< typename Index >
      [[nodiscard]] std::enable_if_t< std::is_convertible_v< Index, std::size_t >, const basic_value* > unsafe_find( Index&& index ) const noexcept
      {
         return ( static_cast< std::size_t >( index ) < m_union.a.size() ) ? ( m_union.a.data() + static_cast< std::size_t >( index ) ) : nullptr;
      }

      template< typename Index >
      [[nodiscard]] std::enable_if_t< std::is_convertible_v< Index, std::size_t >, basic_value* > find( Index&& index )
      {
         validate_json_type( json::type::ARRAY );
         return unsafe_find( index );
      }

      template< typename Index >
      [[nodiscard]] std::enable_if_t< std::is_convertible_v< Index, std::size_t >, const basic_value* > find( Index&& index ) const
      {
         validate_json_type( json::type::ARRAY );
         return unsafe_find( index );
      }

      template< typename Key >
      [[nodiscard]] std::enable_if_t< !std::is_convertible_v< Key, std::size_t >, basic_value* > unsafe_find( Key&& key ) noexcept
      {
         const auto it = m_union.o.find( std::forward< Key >( key ) );
         return ( it != m_union.o.end() ) ? ( &it->second ) : nullptr;
      }

      template< typename Key >
      [[nodiscard]] std::enable_if_t< !std::is_convertible_v< Key, std::size_t >, const basic_value* > unsafe_find( Key&& key ) const noexcept
      {
         const auto it = m_union.o.find( std::forward< Key >( key ) );
         return ( it != m_union.o.end() ) ? ( &it->second ) : nullptr;
      }

      template< typename Key >
      [[nodiscard]] std::enable_if_t< !std::is_convertible_v< Key, std::size_t > && !std::is_convertible_v< Key, pointer >, basic_value* > find( Key&& key )
      {
         validate_json_type( json::type::OBJECT );
         return unsafe_find( std::forward< Key >( key ) );
      }

      template< typename Key >
      [[nodiscard]] std::enable_if_t< !std::is_convertible_v< Key, std::size_t > && !std::is_convertible_v< Key, pointer >, const basic_value* > find( Key&& key ) const
      {
         validate_json_type( json::type::OBJECT );
         return unsafe_find( std::forward< Key >( key ) );
      }

      [[nodiscard]] basic_value* find( const pointer& k )
      {
         return internal::pointer_find( this, k.begin(), k.end() );
      }

      [[nodiscard]] const basic_value* find( const pointer& k ) const
      {
         return internal::pointer_find( this, k.begin(), k.end() );
      }

      template< typename Index >
      [[nodiscard]] std::enable_if_t< std::is_convertible_v< Index, std::size_t >, basic_value& > unsafe_at( Index&& index ) noexcept
      {
         return m_union.a[ index ];
      }

      template< typename Index >
      [[nodiscard]] std::enable_if_t< std::is_convertible_v< Index, std::size_t >, const basic_value& > unsafe_at( Index&& index ) const noexcept
      {
         return m_union.a[ index ];
      }

      template< typename Key >
      [[nodiscard]] std::enable_if_t< !std::is_convertible_v< Key, std::size_t >, basic_value& > unsafe_at( Key&& key ) noexcept
      {
         return m_union.o.find( std::forward< Key >( key ) )->second;
      }

      template< typename Key >
      [[nodiscard]] std::enable_if_t< !std::is_convertible_v< Key, std::size_t >, const basic_value& > unsafe_at( Key&& key ) const noexcept
      {
         return m_union.o.find( std::forward< Key >( key ) )->second;
      }

      template< typename Index >
      [[nodiscard]] std::enable_if_t< std::is_convertible_v< Index, std::size_t >, basic_value& > at( Index&& index )
      {
         validate_json_type( json::type::ARRAY );
         auto& a = m_union.a;
         if( static_cast< std::size_t >( index ) >= a.size() ) {
            throw_index_out_of_bound_exception( index );
         }
         return a[ index ];
      }

      template< typename Index >
      [[nodiscard]] std::enable_if_t< std::is_convertible_v< Index, std::size_t >, const basic_value& > at( Index&& index ) const
      {
         validate_json_type( json::type::ARRAY );
         const auto& a = m_union.a;
         if( static_cast< std::size_t >( index ) >= a.size() ) {
            throw_index_out_of_bound_exception( index );
         }
         return a[ index ];
      }

      template< typename Key >
      [[nodiscard]] std::enable_if_t< !std::is_convertible_v< Key, std::size_t >, basic_value& > at( const Key& key )
      {
         validate_json_type( json::type::OBJECT );
         const auto it = m_union.o.find( key );
         if( it == m_union.o.end() ) {
            throw_key_not_found_exception( key );
         }
         return it->second;
      }

      template< typename Key >
      [[nodiscard]] std::enable_if_t< !std::is_convertible_v< Key, std::size_t >, const basic_value& > at( const Key& key ) const
      {
         validate_json_type( json::type::OBJECT );
         const auto it = m_union.o.find( key );
         if( it == m_union.o.end() ) {
            throw_key_not_found_exception( key );
         }
         return it->second;
      }

      [[nodiscard]] basic_value& at( const pointer& k )
      {
         return internal::pointer_at( this, k.begin(), k.end() );
      }

      [[nodiscard]] const basic_value& at( const pointer& k ) const
      {
         return internal::pointer_at( this, k.begin(), k.end() );
      }

      template< typename Index >
      [[nodiscard]] std::enable_if_t< std::is_convertible_v< Index, std::size_t >, basic_value& > operator[]( Index&& index ) noexcept
      {
         assert( m_type == json::type::ARRAY );
         return m_union.a[ index ];
      }

      template< typename Index >
      [[nodiscard]] std::enable_if_t< std::is_convertible_v< Index, std::size_t >, const basic_value& > operator[]( Index&& index ) const noexcept
      {
         assert( m_type == json::type::ARRAY );
         return m_union.a[ index ];
      }

      template< typename Key >
      [[nodiscard]] std::enable_if_t< !std::is_convertible_v< Key, std::size_t > && !std::is_convertible_v< Key, pointer >, basic_value& > operator[]( Key&& key )
      {
         prepare_object();
         return m_union.o[ std::forward< Key >( key ) ];
      }

      [[nodiscard]] basic_value& operator[]( const pointer& k )
      {
         if( k.empty() ) {
            return *this;
         }
         const auto b = k.begin();
         const auto e = std::prev( k.end() );
         basic_value& v = internal::pointer_at( this, b, e );
         switch( v.m_type ) {
            case json::type::ARRAY: {
               if( e->key() == "-" ) {
                  v.unsafe_emplace_back( null );
                  return v.m_union.a.back();
               }
               return v.at( e->index() );
            }
            case json::type::OBJECT: {
               const auto& key = e->key();
               const auto it = v.m_union.o.find( key );
               if( it != v.m_union.o.end() ) {
                  return it->second;
               }
               const auto r = v.unsafe_try_emplace( key, null );
               assert( r.second );
               return r.first->second;
            }
            default:
               throw internal::invalid_type( b, std::next( e ) );
         }
      }

      template< typename T >
      [[nodiscard]] std::enable_if_t< internal::has_as< Traits< T >, basic_value >, T > as() const
      {
         return Traits< T >::as( *this );
      }

      template< typename T >
      [[nodiscard]] std::enable_if_t< !internal::has_as< Traits< T >, basic_value > && internal::has_as_type< Traits, T, basic_value >, T > as() const
      {
         return Traits< T >::template as_type< Traits, T >( *this );
      }

      template< typename T >
      [[nodiscard]] std::enable_if_t< !internal::has_as< Traits< T >, basic_value > && !internal::has_as_type< Traits, T, basic_value > && internal::has_to< Traits< T >, basic_value, T >, T > as() const
      {
         T v;  // TODO: Should is_default_constructible< T > be part of the enable_if, static_assert()ed here, or this line allowed to error?
         Traits< T >::to( *this, v );
         return v;
      }

      template< typename T >
      std::enable_if_t< !internal::has_as< Traits< T >, basic_value > && !internal::has_as_type< Traits, T, basic_value > && !internal::has_to< Traits< T >, basic_value, T > > as() const = delete;

      template< typename T, typename K >
      [[nodiscard]] T as( const K& key ) const
      {
         return this->at( key ).template as< T >();
      }

      // TODO: Incorporate has_as_type in the following functions (and throughout the library) (if we decide keep it)!

      template< typename T, typename... With >
      [[nodiscard]] std::enable_if_t< internal::has_as< Traits< T >, basic_value, With... >, T > as_with( With&&... with ) const
      {
         return Traits< T >::as( *this, with... );
      }

      template< typename T, typename... With >
      [[nodiscard]] std::enable_if_t< !internal::has_as< Traits< T >, basic_value, With... > && internal::has_to< Traits< T >, basic_value, T, With... >, T > as_with( With&&... with ) const
      {
         T v;  // TODO: Should is_default_constructible< T > be part of the enable_if, static_assert()ed here, or this line allowed to error?
         Traits< T >::to( *this, v, with... );
         return v;
      }

      template< typename T, typename... With >
      std::enable_if_t< !internal::has_as< Traits< T >, basic_value, With... > && !internal::has_to< Traits< T >, basic_value, T, With... >, T > as_with( With&&... with ) const = delete;

      template< typename T >
      std::enable_if_t< internal::has_to< Traits< T >, basic_value, T > > to( T& v ) const
      {
         Traits< std::decay_t< T > >::to( *this, v );
      }

      template< typename T >
      std::enable_if_t< !internal::has_to< Traits< T >, basic_value, T > && internal::has_as< Traits< T >, basic_value > > to( T& v ) const
      {
         v = Traits< std::decay_t< T > >::as( *this );
      }

      template< typename T >
      std::enable_if_t< !internal::has_to< Traits< T >, basic_value, T > && !internal::has_as< Traits< T >, basic_value > > to( T& v ) const = delete;

      template< typename T, typename K >
      void to( T& v, const K& key )
      {
         this->at( key ).to( v );
      }

      template< typename T, typename... With >
      std::enable_if_t< internal::has_to< Traits< T >, basic_value, T, With... > > to_with( T& v, With&&... with ) const
      {
         Traits< std::decay_t< T > >::to( *this, v, with... );
      }

      template< typename T, typename... With >
      std::enable_if_t< !internal::has_to< Traits< T >, basic_value, T, With... > && internal::has_as< Traits< T >, basic_value, With... > > to_with( T& v, With&&... with ) const
      {
         v = Traits< std::decay_t< T > >::as( *this, with... );
      }

      template< typename T, typename... With >
      std::enable_if_t< !internal::has_to< Traits< T >, basic_value, T, With... > && !internal::has_as< Traits< T >, basic_value, With... > > to_with( T& v, With&&... with ) const = delete;

      template< typename T >
      [[nodiscard]] std::optional< T > optional() const
      {
         if( is_null() ) {
            return std::nullopt;
         }
         return as< T >();
      }

      template< typename T, typename K >
      [[nodiscard]] std::optional< T > optional( const K& key ) const
      {
         if( const auto* p = find( key ) ) {
            return p->template as< T >();
         }
         return std::nullopt;
      }

      void erase( const std::size_t index )
      {
         validate_json_type( json::type::ARRAY );
         auto& a = m_union.a;
         if( index >= a.size() ) {
            throw_index_out_of_bound_exception( index );
         }
         a.erase( a.begin() + index );
      }

      template< typename Key >
      std::enable_if_t< !std::is_convertible_v< Key, std::size_t > && !std::is_convertible_v< Key, pointer > > erase( const Key& key )
      {
         validate_json_type( json::type::OBJECT );
         if( m_union.o.erase( key ) == 0 ) {
            throw_key_not_found_exception( key );
         }
      }

      void erase( const pointer& k )
      {
         if( !k ) {
            throw std::runtime_error( internal::format( "invalid root JSON Pointer for erase", json::message_extension( *this ) ) );
         }
         const auto b = k.begin();
         const auto e = std::prev( k.end() );
         basic_value& v = internal::pointer_at( this, b, e );
         switch( v.m_type ) {
            case json::type::ARRAY:
               v.erase( e->index() );
               break;
            case json::type::OBJECT:
               v.erase( e->key() );
               break;
            default:
               throw internal::invalid_type( b, std::next( e ) );
         }
      }

      basic_value& insert( const pointer& k, basic_value value )
      {
         if( !k ) {
            *this = std::move( value );
            return *this;
         }
         const auto b = k.begin();
         const auto e = std::prev( k.end() );
         basic_value& v = internal::pointer_at( this, b, e );
         switch( v.m_type ) {
            case json::type::ARRAY: {
               auto& a = v.m_union.a;
               if( e->key() == "-" ) {
                  v.unsafe_emplace_back( std::move( value ) );
                  return a.back();
               }
               const auto i = e->index();
               if( i >= a.size() ) {
                  throw std::out_of_range( internal::format( "invalid JSON Pointer \"", internal::tokens_to_string( b, std::next( e ) ), "\", array index '", i, "' out of bound '", a.size(), '\'', json::message_extension( *this ) ) );
               }
               a.insert( a.begin() + i, std::move( value ) );
               return a.at( i );
            }
            case json::type::OBJECT: {
               auto& o = v.m_union.o;
               const auto& key = e->key();
               const auto it = o.find( key );
               if( it == o.end() ) {
                  const auto r = v.unsafe_try_emplace( key, std::move( value ) );
                  assert( r.second );
                  return r.first->second;
               }
               it->second = std::move( value );
               return it->second;
            }
            default:
               throw internal::invalid_type( b, std::next( e ) );
         }
      }

      void unsafe_discard() noexcept
      {
         assert( m_type != json::type::DESTROYED );

         switch( m_type ) {
            case json::type::UNINITIALIZED:
            case json::type::DISCARDED:
            case json::type::DESTROYED:
            case json::type::NULL_:
            case json::type::BOOLEAN:
            case json::type::SIGNED:
            case json::type::UNSIGNED:
            case json::type::DOUBLE:
            case json::type::VALUE_PTR:
            case json::type::OPAQUE_PTR:
               return;

            case json::type::STRING:
               std::destroy_at( std::addressof( m_union.s ) );
               return;

            case json::type::STRING_VIEW:
               std::destroy_at( std::addressof( m_union.sv ) );
               return;

            case json::type::BINARY:
               std::destroy_at( std::addressof( m_union.x ) );
               return;

            case json::type::BINARY_VIEW:
               std::destroy_at( std::addressof( m_union.xv ) );
               return;

            case json::type::ARRAY:
               std::destroy_at( std::addressof( m_union.a ) );
               return;

            case json::type::OBJECT:
               std::destroy_at( std::addressof( m_union.o ) );
               return;
         }
         assert( false );  // LCOV_EXCL_LINE
      }

      void discard() noexcept
      {
         unsafe_discard();
         m_type = json::type::DISCARDED;
      }

      void reset() noexcept
      {
         unsafe_discard();
         m_type = json::type::UNINITIALIZED;
      }

      void validate_json_type( const json::type t ) const
      {
         if( m_type != t ) {
            throw std::logic_error( internal::format( "invalid json type '", m_type, "', expected '", t, '\'', json::message_extension( *this ) ) );
         }
      }

   private:
#if defined( __GNUC__ ) && ( __GNUC__ >= 7 )
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif
      void seize( basic_value&& r ) noexcept
      {
         assert( m_type != json::type::DESTROYED );

         switch( r.m_type ) {
            case json::type::UNINITIALIZED:
#ifndef NDEBUG
               r.m_type = json::type::DISCARDED;
#endif
               return;

            case json::type::DISCARDED: {  // LCOV_EXCL_START
               assert( r.m_type != json::type::DISCARDED );
               return;
               // LCOV_EXCL_STOP
            }

            case json::type::DESTROYED: {  // LCOV_EXCL_START
               assert( r.m_type != json::type::DESTROYED );
               return;
               // LCOV_EXCL_STOP
            }

            case json::type::NULL_:
#ifndef NDEBUG
               r.m_type = json::type::DISCARDED;
#endif
               return;

            case json::type::BOOLEAN:
               m_union.b = r.m_union.b;
#ifndef NDEBUG
               r.m_type = json::type::DISCARDED;
#endif
               return;

            case json::type::SIGNED:
               m_union.i = r.m_union.i;
#ifndef NDEBUG
               r.m_type = json::type::DISCARDED;
#endif
               return;

            case json::type::UNSIGNED:
               m_union.u = r.m_union.u;
#ifndef NDEBUG
               r.m_type = json::type::DISCARDED;
#endif
               return;

            case json::type::DOUBLE:
               m_union.d = r.m_union.d;
#ifndef NDEBUG
               r.m_type = json::type::DISCARDED;
#endif
               return;

            case json::type::STRING:
               new( &m_union.s ) std::string( std::move( r.m_union.s ) );
#ifndef NDEBUG
               r.discard();
#endif
               return;

            case json::type::STRING_VIEW:
               new( &m_union.sv ) std::string_view( r.m_union.sv );
#ifndef NDEBUG
               r.m_type = json::type::DISCARDED;
#endif
               return;

            case json::type::BINARY:
               new( &m_union.x ) binary( std::move( r.m_union.x ) );
#ifndef NDEBUG
               r.discard();
#endif
               return;

            case json::type::BINARY_VIEW:
               new( &m_union.xv ) tao::binary_view( r.m_union.xv );
#ifndef NDEBUG
               r.m_type = json::type::DISCARDED;
#endif
               return;

            case json::type::ARRAY:
               new( &m_union.a ) array_t( std::move( r.m_union.a ) );
#ifndef NDEBUG
               r.discard();
#endif
               return;

            case json::type::OBJECT:
               new( &m_union.o ) object_t( std::move( r.m_union.o ) );
#ifndef NDEBUG
               r.discard();
#endif
               return;

            case json::type::VALUE_PTR:
               m_union.p = r.m_union.p;
#ifndef NDEBUG
               r.m_type = json::type::DISCARDED;
#endif
               return;

            case json::type::OPAQUE_PTR:
               m_union.q = r.m_union.q;
#ifndef NDEBUG
               r.m_type = json::type::DISCARDED;
#endif
               return;
         }
         assert( false );  // LCOV_EXCL_LINE
      }
#if defined( __GNUC__ ) && ( __GNUC__ >= 7 )
#pragma GCC diagnostic pop
#endif

      void embed( const basic_value& r )
      {
         switch( r.m_type ) {
            case json::type::UNINITIALIZED:
               return;

            case json::type::DISCARDED:
               throw std::logic_error( "attempt to use a discarded value" );

            case json::type::DESTROYED:
               throw std::logic_error( "attempt to use a destroyed value" );

            case json::type::NULL_:
               return;

            case json::type::BOOLEAN:
               m_union.b = r.m_union.b;
               return;

            case json::type::SIGNED:
               m_union.i = r.m_union.i;
               return;

            case json::type::UNSIGNED:
               m_union.u = r.m_union.u;
               return;

            case json::type::DOUBLE:
               m_union.d = r.m_union.d;
               return;

            case json::type::STRING:
               new( &m_union.s ) std::string( r.m_union.s );
               return;

            case json::type::STRING_VIEW:
               new( &m_union.sv ) std::string_view( r.m_union.sv );
               return;

            case json::type::BINARY:
               new( &m_union.x ) binary( r.m_union.x );
               return;

            case json::type::BINARY_VIEW:
               new( &m_union.xv ) tao::binary_view( r.m_union.xv );
               return;

            case json::type::ARRAY:
               new( &m_union.a ) array_t( r.m_union.a );
               return;

            case json::type::OBJECT:
               new( &m_union.o ) object_t( r.m_union.o );
               return;

            case json::type::VALUE_PTR:
               m_union.p = r.m_union.p;
               return;

            case json::type::OPAQUE_PTR:
               m_union.q = r.m_union.q;
               return;
         }
         assert( false );  // LCOV_EXCL_LINE
      }

      template< typename T >
      static void producer_wrapper( events::virtual_base& consumer, const void* raw )
      {
         Traits< T >::template produce< Traits >( consumer, *static_cast< const T* >( raw ) );
      }

      json::type m_type = json::type::UNINITIALIZED;
      internal::value_union< basic_value > m_union;
   };

}  // namespace tao::json

#endif
