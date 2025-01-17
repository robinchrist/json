// Copyright (c) 2017-2019 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/json/

#ifndef TAO_JSON_MSGPACK_EVENTS_TO_STRING_HPP
#define TAO_JSON_MSGPACK_EVENTS_TO_STRING_HPP

#include <sstream>
#include <string>

#include "to_stream.hpp"

namespace tao::json::msgpack::events
{
   struct to_string
      : to_stream
   {
      std::ostringstream oss;

      to_string()
         : to_stream( oss )
      {}

      [[nodiscard]] std::string value() const
      {
         return oss.str();
      }
   };

}  // namespace tao::json::msgpack::events

#endif
