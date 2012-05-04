// Copyright (C) 2012 Anders Dalle Henning Dalvander
//
// Use, modification and distribution are subject to the Boost Software
// License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(ADHD_JSON_VALUE_H)
#define ADHD_JSON_VALUE_H

#include <boost/utility/enable_if.hpp>
#include <boost/type_traits/is_same.hpp>
#include <boost/static_assert.hpp>
#include <stdexcept>
#include <iosfwd>
#include <string>
#include <vector>
#include <map>
#include <assert.h>

#if !defined(ADHD_JSON_API)
#  ifdef _WIN32
#     if defined(ADHD_JSON_EXPORT)
#        define ADHD_JSON_API __declspec(dllexport)
#     elif defined(ADHD_JSON_IMPORT)
#        define ADHD_JSON_API __declspec(dllimport)
#     else
#        define ADHD_JSON_API
#     endif
#  else
#     define ADHD_JSON_API
#  endif
#endif

namespace adhd
{
   /// Exception thrown by json_parser (see json_parser.h) if parsing fails.
   class ADHD_JSON_API json_parse_exception : public std::runtime_error
   {
   public:
      explicit json_parse_exception(const char* message)
         : std::runtime_error(message)
      {
      }

      virtual ~json_parse_exception();
   };

   /// Represents a JSON value of type null.
   struct json_null
   {
   };

   /// Represents a JSON value of type string.
   struct json_string
   {
      explicit json_string(const std::string& val) : val(val) {}
      std::string val;
   };

   /// Represents a JSON value of type number.
   struct json_number
   {
      explicit json_number(double val) : val(val) {}
      double val;
   };

   /// Represents a JSON value of type bool.
   struct json_bool
   {
      explicit json_bool(bool b) : val(b) {}
      bool val;
   };

   /// Represents a JSON value of type array.
   struct json_array
   {
   };

   /// Represents a JSON value of type object.
   struct json_object
   {
   };

   /// JavaScript Object Notation (JSON) is a lightweight, text-based,
   /// language-independent data interchange format. It was derived from
   /// the ECMAScript Programming Language Standard. JSON defines a small
   /// set of formatting rules for the portable representation of structured
   /// data.
   ///
   /// JSON can represent four primitive types (strings, numbers, booleans,
   /// and null) and two structured types (objects and arrays).
   ///
   /// Where:
   ///    - An object is an unordered collection of zero or more name/value
   ///      pairs, where a name is a string and a value is a string, number,
   ///      boolean, null, object, or array.
   ///
   ///    - An array is an ordered sequence of zero or more values.
   ///
   /// See:
   /// * http://www.json.org/
   /// * http://www.ietf.org/rfc/rfc4627
   ///
   /// This class could be seen as the result of the NIH syndrome. But when
   /// searching the net an existing library wasn't found which had an
   /// acceptable license model, acceptable number of bugs, acceptable
   /// performance, acceptable code bloat and acceptable required features.
   class ADHD_JSON_API json_value
   {
   public:
      json_value()
         : vt(variant_type_null)
         , vd()
      {
      }

      json_value(const json_null& /*val*/)
         : vt(variant_type_null)
         , vd()
      {
      }

      json_value(const json_string& val)
         : vt(variant_type_string)
         , vd(val.val)
      {
      }

      explicit json_value(const std::string& val)
         : vt(variant_type_string)
         , vd(val)
      {
      }

      json_value(const json_number& val)
         : vt(variant_type_number)
         , vd(val.val)
      {
      }

      explicit json_value(double val)
         : vt(variant_type_number)
         , vd(val)
      {
      }

      template <class T>
      explicit json_value(T, typename boost::enable_if< boost::is_same<T, bool> >::type* = 0)
      {
         // prevent direct initialization from bool, use the json_value(json_bool) constructor instead.
         BOOST_STATIC_ASSERT(false);
      }

      json_value(const json_bool& val)
         : vt(variant_type_bool)
         , vd(val)
      {
      }

      json_value(const json_array& val)
         : vt(variant_type_array)
         , vd(val)
      {
      }

      json_value(const json_object& val)
         : vt(variant_type_object)
         , vd(val)
      {
      }

      json_value(const json_value& rhs);

      json_value& operator=(json_value rhs)
      {
         swap(rhs);
         return *this;
      }

      ~json_value();

      void swap(json_value& rhs)
      {
         std::swap(vt, rhs.vt);
         std::swap(vd, rhs.vd);
      }

      /// Recursively visits the visitor.
      template <typename TVisitor>
      void accept(TVisitor& visitor) const
      {
         switch (vt)
         {
         case variant_type_null:
            visitor.null_value();
            break;
         case variant_type_string:
            visitor.string_value(*vd.s);
            break;
         case variant_type_number:
            visitor.number_value(vd.n);
            break;
         case variant_type_bool:
            visitor.bool_value(vd.b);
            break;
         case variant_type_array:
            visitor.begin_array();
            for (variant_data::a_type::const_iterator i = vd.a->begin(), e = vd.a->end(); i != e; ++i)
            {
               visitor.begin_value();
               i->accept(visitor);
               visitor.end_value();
            }
            visitor.end_array();
            break;
         case variant_type_object:
            visitor.begin_object();
            for (variant_data::o_type::const_iterator i = vd.o->begin(), e = vd.o->end(); i != e; ++i)
            {
               visitor.begin_key();
               visitor.string_value(i->first);
               visitor.end_key();
               visitor.begin_value();
               i->second.accept(visitor);
               visitor.end_value();
            }
            visitor.end_object();
            break;
         default:
            break;
         }
      }

      bool operator==(const json_value& rhs) const;

      bool operator!=(const json_value& rhs) const
      {
         return !(*this == rhs);
      }

      bool operator<(const json_value& rhs) const;

      bool operator>(const json_value& rhs) const
      {
         return rhs < *this;
      }

      bool operator<=(const json_value& rhs) const
      {
         return !(rhs > *this);
      }

      bool operator>=(const json_value& rhs) const
      {
         return !(*this < rhs);
      }

      typedef const std::string& (json_value::*unspecified_bool_type)() const;

      operator unspecified_bool_type() const
      {
         return is_null() ? 0 : &json_value::get_string;
      }

      bool operator!() const
      {
         return is_null();
      }

      bool is_null() const
      {
         return vt == variant_type_null;
      }

      bool is_string() const
      {
         return vt == variant_type_string;
      }

      const std::string& get_string() const
      {
         assert(is_string());
         return is_string() ? *vd.s : empty_string;
      }

      bool is_number() const
      {
         return vt == variant_type_number;
      }

      double get_number() const
      {
         assert(is_number());
         return is_number() ? vd.n : 0;
      }

      bool is_bool() const
      {
         return vt == variant_type_bool;
      }

      bool get_bool() const
      {
         assert(is_bool());
         return is_bool() ? vd.b : false;
      }

      bool is_array() const
      {
         return vt == variant_type_array;
      }

      const json_value& get_child(size_t i) const;

      json_value& put_child(size_t i);

      json_value& append_child()
      {
         return put_child(get_length());
      }

      size_t get_length() const
      {
         return is_array() ? vd.a->size() : 0;
      }

      void set_length(size_t length);

      bool is_object() const
      {
         return vt == variant_type_object;
      }

      const json_value& get_child(const std::string& name) const;

      json_value& put_child(const std::string& name);

      bool has_child(const std::string& name) const;

      bool erase_child(const std::string& name);

      std::string to_pretty_string(size_t indent_size = 4) const;

      std::ostream& pretty_print(std::ostream& os, size_t indent_size = 4) const;

      std::string to_string() const;

      /// Predicate to check if a character should be escaped.
      struct need_escaping
      {
         bool operator()(char c) const
         {
            return (c >= 0 && c < 0x20) || c == 0x7f || c == '\\' || c == '\"';
         }
      };

      /// Public static for representing a null JSON value.
      static const json_value null;

   private:
      static const std::string empty_string;

      enum variant_type
      {
         variant_type_null,
         variant_type_string,
         variant_type_number,
         variant_type_bool,
         variant_type_array,
         variant_type_object,
      };

      union variant_data
      {
         variant_data()
            : v()
         {
         }

         variant_data(const std::string& val)
            : s(new std::string(val))
         {
         }

         variant_data(double val)
            : n(val)
         {
         }

         variant_data(const json_bool& val)
            : b(val.val)
         {
         }

         variant_data(const json_array& /*val*/)
            : a(new a_type())
         {
         }

         variant_data(const json_object& /*val*/)
            : o(new o_type())
         {
         }

         typedef std::vector<json_value> a_type;
         typedef std::map<std::string, json_value> o_type;

         void* v;
         std::string* s;
         double n;
         bool b;
         a_type* a;
         o_type* o;
      };

      variant_type vt;
      variant_data vd;
   };

   /// Outputs a JSON value to a stream in a compact way.
   ADHD_JSON_API std::ostream& operator<<(std::ostream& os, const json_value& rhs);
}

#endif
