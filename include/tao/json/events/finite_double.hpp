// Copyright (c) 2017 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/json/

#ifndef TAOCPP_JSON_INCLUDE_EVENTS_FINITE_DOUBLE_HPP
#define TAOCPP_JSON_INCLUDE_EVENTS_FINITE_DOUBLE_HPP

namespace tao
{
   namespace json
   {
      namespace events
      {
         template< typename Consumer >
         struct finite_double
            : public Consumer
         {
            using Consumer::Consumer;

            using Consumer::number;

            void number( const double v )
            {
               if( !std::isfinite( v ) ) {
                  Consumer::null();
               }
               else {
                  Consumer::number( v );
               }
            }
         };

      }  // events

   }  // json

}  // tao

#endif