/**
 * @file config-bits.hh
 *
 * Code in this file contains implementational details from config.hh which needn't
 * obfuscate the config.hh include header.
 */
#ifndef __BITS_CONFIG_HH_
#define __BITS_CONFIG_HH_

#include <cassert>
#include <cstring>
#include <array>
#include <initializer_list>
#include <map>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>


//////////////////////////////////////////////////////////////////////////////////////////
/// TODO: encapsulate this internal iterator type info better
typedef std::string::iterator _Iter;

inline bool
acceptable_char(char c) {
    return std::isalpha(c) || std::isdigit(c) || c == '_';
}

//////////////////////////////////////////////////////////////////////////////////////////

class trie_lookup_error : public std::runtime_error {
public:
    trie_lookup_error()
        : std::runtime_error("TRIE PARSE ERROR")
    {}
};

class trie_define_error : public std::runtime_error {
public:
    trie_define_error()
        : std::runtime_error("TRIE DEFINE ERROR")
    {}
};

//////////////////////////////////////////////////////////////////////////////////////////
/**
 * @class       parse_trie
 * @template    _ValType "stored type"
 *
 * This class is a non-compact trie structure which should not be used for high
 * performance access.
 *
 * @complexity O(len(key_name))
 */
template <typename _ValType>
class parse_trie {
public:
    parse_trie() = default;
    parse_trie(std::initializer_list<std::pair<std::string, _ValType>> init) {
        for (auto it = init.begin(); it != init.end(); ++it)
            this->defval(it->first) = it->second;
    }

    ~parse_trie() {
        for (auto it = _M_paths.begin(); it != _M_paths.end(); ++it)
            delete it->second;
    }

    _ValType&
    defval(std::string key) {
        auto it = key.begin();
        return this->define(it);
    }

    template <typename _Iter>
    _ValType&
    define(_Iter& iter) {
        const char index = _S_index_char(*iter);

        if (! acceptable_char(*iter)) {
            if ('\0' == *iter)
                return _M_data;
            else
                throw trie_define_error();
        }

        parse_trie* ptr(0x0);
        auto it = _M_paths.find(index);

        if (it == _M_paths.end())
            _M_paths[index] = ptr = new parse_trie();
        else
            ptr = it->second;

        return ptr->define(++iter);
    }

    template <typename _Iter>
    _ValType
    lookup(_Iter& iter) {
        auto it = _M_lookup(iter);

        if (! it.first)
            throw trie_lookup_error();
        else
            return it.second;
    }

private:
    static int
    _S_index_char(char c) { return std::toupper(c); }

    //----------------------------------------------------------------------------------//
    ///{@
    /**
     * HACK:
     * This struct template is used to for the two _M_is_terminal functions below to
     * allow for two _ValType's :
     *     o those with a .size() member
     *     o those which are castable with bool()
     *
     * If the parse_trie type needs to support a type in which neither case is sane this
     * class needs to be re-written with a predicate.
     *
     * @struct has_size_method
     * This structure works as a specific variant of SFINAE
     */
    template <typename _K>
    struct has_size_method {
        typedef char y;
        struct n { char _[2]; };

        template <typename _T, size_t(_T::*)() const = &_T::size>
        static y impl(_T*);
        static n impl(...);

        static constexpr bool value = sizeof(impl(static_cast<_K*>(0x0))) == sizeof(y);
    };

    template <typename _Tp = _ValType>
    typename std::enable_if<has_size_method<_Tp>::value, bool>::type
    _M_is_terminal() const {
        return 0 != _M_data.size();
    }

    template <typename _Tp = _ValType>
    typename std::enable_if<!has_size_method<_Tp>::value, bool>::type
    _M_is_terminal() const {
        return static_cast<bool>(_M_data);
    }
    ///@}
    //----------------------------------------------------------------------------------//


    template <typename _Iter>
    std::pair<bool, _ValType>
    _M_lookup(_Iter& iter) {
        const char index = _S_index_char(*iter);

        switch (*iter) {
            case '{':
                return _M_lookup(++iter);

            case '\0':
            case ' ' :
            case '\t':
            case '\n':
            case '\r':
            case '}':
                if (_M_is_terminal())
                    return std::make_pair(true, _M_data);
                else
                    return std::make_pair(false, _M_data);

            default:
                break;
        }

        auto lookupv = _M_paths.find(index);

        if (lookupv == _M_paths.end())
            return std::make_pair(_M_is_terminal(), _M_data);

        auto it = _M_paths[index]->_M_lookup(++iter);

        if (it.first) {
            /* somewhere down the line a match was found */
            return it;
        } else {
            /* there was no match for the next character */
            --iter;
            return std::make_pair(_M_is_terminal(), _M_data);
        }
    }

    _ValType _M_data;
    std::map<char, parse_trie*> _M_paths;
};

//////////////////////////////////////////////////////////////////////////////////////////

#endif //__BITS_CONFIG_HH_
