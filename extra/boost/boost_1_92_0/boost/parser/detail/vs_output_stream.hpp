#ifndef BOOST_PARSER_DETAIL_VS_OUTPUT_STREAM_HPP
#define BOOST_PARSER_DETAIL_VS_OUTPUT_STREAM_HPP

#include <array>
#include <functional>
#include <iostream>
#include <sstream>

#include <Windows.h>


namespace boost::parser::detail {

    template<class CharT, class TraitsT = std::char_traits<CharT>>
    class basic_debugbuf : public std::basic_stringbuf<CharT, TraitsT>
    {
    public:
        virtual ~basic_debugbuf() { sync_impl(); }

    protected:
        int sync_impl()
        {
            output_debug_string(this->str().c_str());
            this->str(std::basic_string<CharT>()); // Clear the string buffer
            return 0;
        }

        int sync() override { return sync_impl(); }

        void output_debug_string(const CharT * text);
    };

    template<>
    inline void basic_debugbuf<char>::output_debug_string(const char * text)
    {
        // Save in-memory logging buffer to a log file on error (from MSDN
        // example).
        std::wstring dest;
        int convert_result = MultiByteToWideChar(
            CP_UTF8, 0, text, static_cast<int>(std::strlen(text)), nullptr, 0);
        if (convert_result <= 0) {
            // cannot convert to wide-char -> use ANSI API
            ::OutputDebugStringA(text);
        } else {
            dest.resize(convert_result + 10);
            convert_result = MultiByteToWideChar(
                CP_UTF8,
                0,
                text,
                static_cast<int>(std::strlen(text)),
                dest.data(),
                static_cast<int>(dest.size()));
            if (convert_result <= 0) {
                // cannot convert to wide-char -> use ANSI API
                ::OutputDebugStringA(text);
            } else {
                ::OutputDebugStringW(dest.c_str());
            }
        }
    }

    template<class CharT, class TraitsT = std::char_traits<CharT>>
    class basic_debug_ostream : public std::basic_ostream<CharT, TraitsT>
    {
    public:
        basic_debug_ostream() :
            std::basic_ostream<CharT, TraitsT>(
                new basic_debugbuf<CharT, TraitsT>())
        {}
        ~basic_debug_ostream() { delete this->rdbuf(); }
    };

    inline basic_debug_ostream<char> vs_cout;
    //
}

#endif
