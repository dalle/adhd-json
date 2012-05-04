// Copyright (C) 2012 Anders Dalle Henning Dalvander
//
// Use, modification and distribution are subject to the Boost Software
// License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(ADHD_JSON_PARSER_H)
#define ADHD_JSON_PARSER_H

#include "json_value.h"
#include <sstream>
#include <stack>
#include <math.h>

namespace adhd
{
   /// Visitor for building a JSON value.
   struct json_builder
   {
      std::stack<json_value*> s;
      json_value keyvalue;
      std::string key;

      json_builder(json_value& root)
         : s()
      {
         s.push(&root);
      }

      void null_value()
      {
         *s.top() = json_null();
      }

      void string_value(const std::string& val)
      {
         *s.top() = json_string(val);
      }

      void number_value(double val)
      {
         *s.top() = json_number(val);
      }

      void bool_value(bool val)
      {
         *s.top() = json_bool(val);
      }

      void begin_array()
      {
         *s.top() = json_array();
      }

      void end_array()
      {
      }

      void begin_object()
      {
         *s.top() = json_object();
      }

      void end_object()
      {
      }

      void begin_key()
      {
         s.push(&keyvalue);
      }

      void end_key()
      {
         key = s.top()->get_string();
         s.pop();
      }

      void begin_value()
      {
         if (s.top()->is_object())
            s.push(&s.top()->put_child(key));
         else
            s.push(&s.top()->append_child());
      }

      void end_value()
      {
         s.pop();
      }
   };

   /// Parser to convert a JSON document into a JSON value.
   class json_parser
   {
   public:
      json_value parse(const std::string& str)
      {
         return parse(str.c_str());
      }

      template <typename TIterator>
      json_value parse(TIterator iter)
      {
         json_value root;
         parse(iter, root);
         return root;
      }

      template <typename TIterator>
      void parse(TIterator iter, json_value& root)
      {
         json_builder builder(root);
         parse(iter, builder);
      }

      template <typename TIterator, typename TVisitor>
      void parse(TIterator iter, TVisitor& visitor)
      {
         skip_whitespace(iter);

         switch (*iter)
         {
         case '{':
            parse_object(iter, visitor);
            break;

         case '[':
            parse_array(iter, visitor);
            break;

         default:
            throw json_parse_exception("expected object or array");
         }

         skip_whitespace(iter);

         if (*iter != '\0')
         {
            throw json_parse_exception("expected end");
         }
      }

   private:
      template <typename TIterator, typename TVisitor>
      void parse_object(TIterator& iter, TVisitor& visitor)
      {
         assert(*iter == '{');
         ++iter; // Skip '{'

         visitor.begin_object();
         skip_whitespace(iter);

         // Is it an empty object?
         if (*iter == '}')
         {
            ++iter;
            visitor.end_object();
            return;
         }

         for (;;)
         {
            if (*iter != '"')
            {
               throw json_parse_exception("expected string");
            }

            visitor.begin_key();
            parse_string(iter, visitor);
            visitor.end_key();

            skip_whitespace(iter);

            if (*iter++ != ':')
            {
               throw json_parse_exception("expected name-separator");
            }

            skip_whitespace(iter);

            visitor.begin_value();
            parse_value(iter, visitor);
            visitor.end_value();

            skip_whitespace(iter);

            switch (*iter++)
            {
            case ',':
               skip_whitespace(iter);
               break;

            case '}':
               visitor.end_object();
               return;

            default:
               throw json_parse_exception("expected value-separator or end-object");
            }
         }
      }

      template <typename TIterator, typename TVisitor>
      void parse_array(TIterator& iter, TVisitor& visitor)
      {
         assert(*iter == '[');
         ++iter; // Skip '['

         visitor.begin_array();
         skip_whitespace(iter);

         // Is it an empty array?
         if (*iter == ']')
         {
            ++iter;
            visitor.end_array();
            return;
         }

         for (;;)
         {
            visitor.begin_value();
            parse_value(iter, visitor);
            visitor.end_value();

            skip_whitespace(iter);

            switch (*iter++)
            {
            case ',':
               skip_whitespace(iter);
               break;

            case ']':
               visitor.end_array();
               return;

            default:
               throw json_parse_exception("expected value-separator or end-array");
            }
         }
      }

      template <typename TIterator, typename TVisitor>
      void parse_null(TIterator& iter, TVisitor& visitor)
      {
         assert(*iter == 'n');
         ++iter;

         if (*iter++ == 'u' && *iter++ == 'l' && *iter++ == 'l')
         {
            visitor.null_value();
         }
         else
         {
            throw json_parse_exception("expected value");
         }
      }

      template <typename TIterator, typename TVisitor>
      void parse_true(TIterator& iter, TVisitor& visitor)
      {
         assert(*iter == 't');
         ++iter;

         if (*iter++ == 'r' && *iter++ == 'u' && *iter++ == 'e')
         {
            visitor.bool_value(true);
         }
         else
         {
            throw json_parse_exception("expected value");
         }
      }

      template <typename TIterator, typename TVisitor>
      void parse_false(TIterator& iter, TVisitor& visitor)
      {
         assert(*iter == 'f');
         ++iter;

         if (*iter++ == 'a' && *iter++ == 'l' && *iter++ == 's' && *iter++ == 'e')
         {
            visitor.bool_value(false);
         }
         else
         {
            throw json_parse_exception("expected value");
         }
      }

      template <typename TIterator>
      unsigned parse_fourhex(TIterator& iter)
      {
         unsigned codepoint = 0;

         for (int i = 0; i < 4; ++i)
         {
            codepoint <<= 4;

            const char c = *iter;

            if (c >= '0' && c <= '9')
            {
               codepoint += c - '0';
            }
            else if (c >= 'A' && c <= 'F')
            {
               codepoint += 10 + c - 'A';
            }
            else if (c >= 'a' && c <= 'f')
            {
               codepoint += 10 + c - 'a';
            }
            else
            {
               throw json_parse_exception("expected 4hexdig");
            }

            ++iter;
         }

         return codepoint;
      }

      // Parse string, handling the prefix and suffix double quotes and escaping.
      template <typename TIterator, typename TVisitor>
      void parse_string(TIterator& iter, TVisitor& visitor)
      {
         assert(*iter == '\"');
         ++iter; // Skip '\"'

         std::ostringstream ss;

         for (;;)
         {
            char c = *iter++;

            if (c == '\\')
            {
               // Escape character
               char e = *iter++;
               switch (e)
               {
               case '\"':
                  ss << '\"';
                  break;
               case '/':
                  ss << '/';
                  break;
               case '\\':
                  ss << '\\';
                  break;
               case 'b':
                  ss << '\b';
                  break;
               case 'f':
                  ss << '\f';
                  break;
               case 'n':
                  ss << '\n';
                  break;
               case 'r':
                  ss << '\r';
                  break;
               case 't':
                  ss << '\t';
                  break;
               case 'u':
                  {
                     // Escaped unicode
                     unsigned codepoint = parse_fourhex(iter);
                     if (codepoint >= 0xd800u && codepoint <= 0xdbffu)
                     {
                        // Handle UTF-16 surrogate pairs
                        if (*iter++ != '\\' || *iter++ != 'u')
                        {
                           throw json_parse_exception("expected trailing surrogate");
                        }

                        const unsigned codepoint2 = parse_fourhex(iter);
                        if (codepoint2 < 0xdc00u || codepoint2 > 0xdfffu)
                        {
                           throw json_parse_exception("expected trailing surrogate");
                        }

                        codepoint = (((codepoint - 0xd800u) << 10) | (codepoint2 - 0xdc00u)) + 0x10000u;
                     }
                     else if (codepoint >= 0xdc00u && codepoint <= 0xdfffu)
                     {
                        throw json_parse_exception("unexpected trailing surrogate.");
                     }

                     if (codepoint < 0x80u)
                     {
                        ss << static_cast<unsigned char>(codepoint);
                     }
                     else if (codepoint < 0x800u)
                     {
                        ss << static_cast<unsigned char>((codepoint >> 6) & 0x1fu | 0xc0u);
                        ss << static_cast<unsigned char>((codepoint & 0x3fu) | 0x80u);
                     }
                     else if (codepoint < 0x10000u)
                     {
                        ss << static_cast<unsigned char>((codepoint >> 12) & 0x0fu | 0xe0u);
                        ss << static_cast<unsigned char>(((codepoint >> 6) & 0x3fu) | 0x80u);
                        ss << static_cast<unsigned char>((codepoint & 0x3fu) | 0x80u);
                     }
                     else
                     {
                        ss << static_cast<unsigned char>((codepoint >> 18) & 0x07u | 0xf0u);
                        ss << static_cast<unsigned char>(((codepoint >> 12) & 0x3fu) | 0x80u);
                        ss << static_cast<unsigned char>(((codepoint >> 6) & 0x3fu) | 0x80u);
                        ss << static_cast<unsigned char>((codepoint & 0x3fu) | 0x80u);
                     }
                  }
                  break;
               default:
                  throw json_parse_exception("expected escape");
               }
            }
            else if (c == '\"')
            {
               visitor.string_value(ss.str());
               return;
            }
            else if (c == '\0')
            {
               throw json_parse_exception("expected char or quotation-mark");
            }
            else if (json_value::need_escaping()(c)) //else if (static_cast<unsigned char>(c) < 0x20)
            {
               throw json_parse_exception("expected char");
            }
            else
            {
               ss << c;
            }
         }
      }

      template <typename TIterator, typename TInteger>
      void parse_int(TIterator& iter, TInteger& integer)
      {
         while (*iter >= '0' && *iter <= '9')
         {
            const int digit = *iter++ - '0';
            integer = integer * 10 + digit;
         }
      }

      template <typename TIterator>
      double parse_fraction(TIterator& iter)
      {
         double fraction = 0;
         double factor = 0.1;

         while (*iter >= '0' && *iter <= '9')
         {
            const int digit = *iter++ - '0';
            fraction = fraction + digit * factor;
            factor *= 0.1;
         }

         return fraction;
      }

      template <typename TIterator, typename TVisitor>
      void parse_number(TIterator& iter, TVisitor& visitor)
      {
         // Check sign.
         const bool minus = *iter == '-';
         if (minus)
         {
            ++iter;
         }

         double number = 0;

         // Is it zero or an integer?
         if (*iter == '0')
         {
            ++iter;
         }
         else if (*iter >= '1' && *iter <= '9')
         {
            parse_int(iter, number);
         }
         else
         {
            throw json_parse_exception("expected integer");
         }

         // Is it a fraction?
         if (*iter == '.')
         {
            ++iter;

            // Only digits are allowed.
            if (!(*iter >= '0' && *iter <= '9'))
            {
               throw json_parse_exception("expected fraction");
            }

            // Parse and add fraction.
            number += parse_fraction(iter);
         }

         // Is it an exponent?
         if (*iter == 'e' || *iter == 'E')
         {
            ++iter;

            // Check sign of exponent.
            const bool negative_exponent = *iter == '-';
            if (*iter == '-' || *iter == '+')
            {
               ++iter;
            }

            // Only digits are allowed.
            if (!(*iter >= '0' && *iter <= '9'))
            {
               throw json_parse_exception("expected exponent");
            }

            // Parse and apply exponent.
            int exponent = 0;
            parse_int(iter, exponent);
            number *= pow(10.0, negative_exponent ? -exponent : exponent);
         }

         visitor.number_value(minus ? -number : number);
      }

      template <typename TIterator, typename TVisitor>
      void parse_value(TIterator& iter, TVisitor& visitor)
      {
         switch (*iter)
         {
         case 'n':
            parse_null(iter, visitor);
            break;

         case 't':
            parse_true(iter, visitor);
            break;

         case 'f':
            parse_false(iter, visitor);
            break;

         case '"':
            parse_string(iter, visitor);
            break;

         case '{':
            parse_object(iter, visitor);
            break;

         case '[':
            parse_array(iter, visitor);
            break;

         default:
            parse_number(iter, visitor);
            break;
         }
      }

      template <typename TIterator>
      void skip_whitespace(TIterator& iter)
      {
         while (*iter == ' ' || *iter == '\n' || *iter == '\r' || *iter == '\t')
         {
            ++iter;
         }
      }
   };
}

#endif
