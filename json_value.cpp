// Copyright (C) 2012 Anders Dalle Henning Dalvander
//
// Use, modification and distribution are subject to the Boost Software
// License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "json_value.h"
#include <algorithm>
#include <sstream>
#include <float.h>

#if defined(_MSC_VER)
#  include <locale.h>
#else
#  include <xlocale.h>
#endif

namespace
{
   /// Helper for quoting strings.
   void json_write_quoted_string(std::ostream& os, const std::string& str)
   {
      static const char* hex = "0123456789abcdef";

      os << '"';

      const char* begin = str.data();
      const char* end = begin + str.size();

      for (const char* p = begin; p != end;)
      {
         const char* e = std::find_if(p, end, adhd::json_value::need_escaping());
         os.write(p, e - p);
         p = e;
         if (p != end)
         {
            switch (*p)
            {
            case '\"':
               os << "\\\"";
               break;
            case '\\':
               os << "\\\\";
               break;
            case '\b':
               os << "\\b";
               break;
            case '\f':
               os << "\\f";
               break;
            case '\n':
               os << "\\n";
               break;
            case '\r':
               os << "\\r";
               break;
            case '\t':
               os << "\\t";
               break;
            default:
               os << "\\u00" << hex[static_cast<unsigned char>(*p) >> 4] << hex[static_cast<unsigned char>(*p) & 0xfu];
               break;
            }

            ++p;
         }
      }

      os << '"';
   }

   /// Helper for writing numbers.
   void json_write_number(std::ostream& os, double d)
   {
      switch (_fpclass(d))
      {
      default:
         assert(!"Unknown floating point classification.");
      case _FPCLASS_SNAN:  // signaling NaN
      case _FPCLASS_QNAN:  // quiet NaN
         os << "null";
         break;
      case _FPCLASS_NINF:  // negative infinity
         os << "\"-inf\"";
         break;
      case _FPCLASS_PINF:  // positive infinity
         os << "\"+inf\"";
         break;
      case _FPCLASS_ND:    // negative denormal
      case _FPCLASS_NZ:    // -0
      case _FPCLASS_PZ:    // +0
      case _FPCLASS_PD:    // positive denormal
         os << "0";
         break;
      case _FPCLASS_NN:    // negative normal
      case _FPCLASS_PN:    // positive normal
         {
            char buffer[100];
#if defined(_MSC_VER)
            struct cached_locale
            {
               cached_locale() : c_locale(_create_locale(LC_ALL, "C")) {}
               ~cached_locale() { _free_locale(c_locale); }
               const _locale_t c_locale;
            };
            static const cached_locale locale_cache;
            const int ret = _sprintf_s_l(buffer, sizeof(buffer), "%.16g", locale_cache.c_locale, d);
#else
            struct cached_locale
            {
               cached_locale() : c_locale(newlocale(LC_ALL_MASK, NULL, NULL)) {}
               ~cached_locale() { freelocale(c_locale); }
               const locale_t c_locale;
            };
            static const cached_locale locale_cache;
            const int ret = snprintf_l(buffer, sizeof(buffer), locale_cache.c_locale, "%.16g", d);
#endif
            for (int i = 0; i < ret; ++i)
            {
               os << buffer[i];
            }
         }
         break;
      }
   }
}

namespace adhd
{
   json_parse_exception::~json_parse_exception()
   {
   }

   /// Visitor for streaming a JSON value in a compact way.
   struct json_writer
   {
      enum skip_state
      {
         skip_none,
         skip_comma,
      };

      std::ostream& os;
      skip_state skip;

      explicit json_writer(std::ostream& os)
         : os(os)
         , skip(skip_comma)
      {
      }

      void null_value()
      {
         os << "null";
      }

      void string_value(const std::string& val)
      {
         json_write_quoted_string(os, val);
      }

      void number_value(double val)
      {
         json_write_number(os, val);
      }

      void bool_value(bool val)
      {
         os << (val ? "true" : "false");
      }

      void begin_array()
      {
         os << '[';
         skip = skip_comma;
      }

      void end_array()
      {
         os << ']';
         skip = skip_none;
      }

      void begin_object()
      {
         os << '{';
         skip = skip_comma;
      }

      void end_object()
      {
         os << '}';
         skip = skip_none;
      }

      void begin_key()
      {
         if (skip == skip_comma)
            skip = skip_none;
         else
            os << ',';
      }

      void end_key()
      {
         os << ':';
         skip = skip_comma;
      }

      void begin_value()
      {
         if (skip == skip_comma)
            skip = skip_none;
         else
            os << ',';
      }

      void end_value()
      {
      }
   };

   /// Visitor for streaming a JSON value in a pretty way, using newlines and indents.
   struct json_pretty_printer
   {
      enum skip_state
      {
         skip_none,
         skip_comma_xor_newline,
         skip_comma_and_newline,
      };

      std::ostream& os;
      skip_state skip;
      int indent_level;
      const std::string indent;

      json_pretty_printer(std::ostream& os, size_t indent_size)
         : os(os)
         , skip(skip_comma_and_newline)
         , indent_level(0)
         , indent(indent_size, ' ')
      {
      }

      void newline()
      {
         os << '\n';

         for (int i = 0; i < indent_level; ++i)
         {
            os << indent;
         }
      }

      void null_value()
      {
         os << "null";
      }

      void string_value(const std::string& val)
      {
         json_write_quoted_string(os, val);
      }

      void number_value(double val)
      {
         json_write_number(os, val);
      }

      void bool_value(bool val)
      {
         os << (val ? "true" : "false");
      }

      void begin_array()
      {
         os << '[';
         skip = skip_comma_xor_newline;
         ++indent_level;
      }

      void end_array()
      {
         --indent_level;
         if (skip == skip_none)
         {
            newline();
         }

         os << ']';
         skip = skip_none;
      }

      void begin_object()
      {
         os << '{';
         skip = skip_comma_and_newline;
         ++indent_level;
      }

      void end_object()
      {
         --indent_level;
         if (skip == skip_none)
         {
            newline();
         }

         os << '}';
         skip = skip_none;
      }

      void begin_key()
      {
         if (skip == skip_none)
         {
            os << ',';
         }

         newline();
         skip = skip_none;
      }

      void end_key()
      {
         os << ':';
         os << ' ';
         skip = skip_comma_and_newline;
      }

      void begin_value()
      {
         if (skip == skip_none)
         {
            os << ',';
            newline();
         }
         else if (skip == skip_comma_xor_newline)
         {
            newline();
         }

         skip = skip_none;
      }

      void end_value()
      {
      }
   };

   const json_value json_value::null;
   const std::string json_value::empty_string;

   json_value::json_value(const json_value& rhs)
      : vt(rhs.vt)
      , vd(rhs.vd)
   {
      switch (vt)
      {
      case variant_type_string:
         vd.s = new std::string(*vd.s);
         break;
      case variant_type_array:
         vd.a = new variant_data::a_type(*vd.a);
         break;
      case variant_type_object:
         vd.o = new variant_data::o_type(*vd.o);
         break;
      default:
         break;
      }
   }

   json_value::~json_value()
   {
      switch (vt)
      {
      case variant_type_string:
         delete vd.s;
         break;
      case variant_type_array:
         delete vd.a;
         break;
      case variant_type_object:
         delete vd.o;
         break;
      default:
         break;
      }
   }

   bool json_value::operator==(const json_value& rhs) const
   {
      if (vt != rhs.vt)
         return false;

      switch (vt)
      {
      case variant_type_null:
         return true;
      case variant_type_string:
         return *vd.s == *rhs.vd.s;
      case variant_type_number:
         return _isnan(vd.n) && _isnan(rhs.vd.n) || vd.n == rhs.vd.n;
      case variant_type_bool:
         return vd.b == rhs.vd.b;
      case variant_type_array:
         return *vd.a == *rhs.vd.a;
      case variant_type_object:
         return *vd.o == *rhs.vd.o;
      default:
         return true;
      }
   }

   bool json_value::operator<(const json_value& rhs) const
   {
      if (vt < rhs.vt)
         return true;

      if (rhs.vt < vt)
         return false;

      switch (vt)
      {
      case variant_type_null:
         return false;
      case variant_type_string:
         return *vd.s < *rhs.vd.s;
      case variant_type_number:
         return !_isnan(rhs.vd.n) && (_isnan(vd.n) || vd.n < rhs.vd.n);
      case variant_type_bool:
         return vd.b < rhs.vd.b;
      case variant_type_array:
         return *vd.a < *rhs.vd.a;
      case variant_type_object:
         return *vd.o < *rhs.vd.o;
      default:
         return false;
      }
   }

   const json_value& json_value::get_child(size_t i) const
   {
      assert(is_null() || is_array());
      if (!is_array())
      {
         return null;
      }

      if (i >= vd.a->size())
      {
         return null;
      }

      return (*vd.a)[i];
   }

   json_value& json_value::put_child(size_t i)
   {
      assert(is_null() || is_array());
      if (!is_array())
      {
         json_value(json_array()).swap(*this);
      }

      if (i >= vd.a->size())
      {
         vd.a->resize(i + 1);
      }

      return (*vd.a)[i];
   }

   void json_value::set_length(size_t length)
   {
      assert(is_null() || is_array());
      if (!is_array())
      {
         json_value(json_array()).swap(*this);
      }

      vd.a->resize(length);
   }

   const json_value& json_value::get_child(const std::string& name) const
   {
      assert(is_null() || is_object());
      if (!is_object())
      {
         return null;
      }

      const variant_data::o_type::const_iterator i = vd.o->find(name);
      if (i == vd.o->end())
      {
         return null;
      }

      return i->second;
   }

   json_value& json_value::put_child(const std::string& name)
   {
      assert(is_null() || is_object());
      if (!is_object())
      {
         json_value(json_object()).swap(*this);
      }

      return vd.o->insert(std::make_pair(name, null)).first->second;
   }

   bool json_value::has_child(const std::string& name) const
   {
      if (!is_object())
      {
         return false;
      }

      return vd.o->find(name) != vd.o->end();
   }

   bool json_value::erase_child(const std::string& name)
   {
      assert(is_null() || is_object());
      if (!is_object())
      {
         return false;
      }

      return vd.o->erase(name) != 0;
   }

   std::string json_value::to_pretty_string(size_t indent_size) const
   {
      std::ostringstream ss;
      pretty_print(ss, indent_size);
      return ss.str();
   }

   std::ostream& json_value::pretty_print(std::ostream& os, size_t indent_size) const
   {
      json_pretty_printer writer(os, indent_size);
      accept(writer);
      return os;
   }

   std::string json_value::to_string() const
   {
      std::ostringstream ss;
      ss << *this;
      return ss.str();
   }

   std::ostream& operator<<(std::ostream& os, const json_value& rhs)
   {
      json_writer writer(os);
      rhs.accept(writer);
      return os;
   }
}
