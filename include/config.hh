#if 0
Copyright 2013 CjHanks <develop@cjhanks.name>

This file is part of libconf.

libconf is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

libconf is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with libconf.  If not, see <http://www.gnu.org/licenses/>.
#endif 

#ifndef __CONFIG_HH_
#define __CONFIG_HH_

#include <iterator>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <sstream>
#include <unordered_map>
#include <vector>

#include "bits/config.hh"
#include "bits/debug.hh"


////////////////////////////////////////////////////////////////////////////////////////// 
#if defined(CONFIG_SINGLETON)
#  define CFG config::instance()
#endif 

using _Iter = std::string::iterator;

class config_io_error : public std::runtime_error {
public:
    config_io_error(std::string fname)
        : std::runtime_error(fname)
    {}
};

class config_key_error : public std::runtime_error {
public:
    config_key_error(std::string key)
        : std::runtime_error(key)
    {}
};

class config_parse_exception : public std::runtime_error {
public:
    config_parse_exception(std::string fname)
        : std::runtime_error(fname)
    {}
    
    config_parse_exception(std::string key, _Iter iter, size_t lsize = 12
                         , size_t rsize = 12)
        : std::runtime_error(parse_error(key, iter, lsize, rsize))
    {}

private:
    static std::string 
    parse_error(std::string key, _Iter iter, size_t lsize, size_t rsize) {
        std::stringstream ss;

        ss << "ERROR [" << key << "]" << std::endl;
        ss << "  --> '" << std::string((iter - lsize), (iter + rsize)) << "'";
        return ss.str();
    }
};


//////////////////////////////////////////////////////////////////////////////////////////
// KWARG ACCESS
//////////////////////////////////////////////////////////////////////////////////////////

class kwarg {
public:
    enum TYPE { FLOATING, INTEGRAL, STRING, SECTION, UNDEFINED, VECTOR, BOOL };

    kwarg(const std::string& name, TYPE type)
        : _M_name(name), _M_type(type)
    {}

    virtual ~kwarg() {}

    std::string name() const { return _M_name; }
    
    TYPE type() const { return _M_type; }

    kwarg& operator=(const kwarg&) = delete;
    kwarg(const kwarg&) = delete;
    kwarg(kwarg&&) = delete;

protected: 
    template <typename _K>
    struct is_bool {
        static constexpr bool value = std::is_same<_K, bool>::value;
    };

    template <typename _K>
    struct is_integral {
        static constexpr bool value = !is_bool<_K>::value 
                                    && std::is_integral<_K>::value;
    };

    template <typename _K>
    struct is_floating{
        static constexpr bool value = std::is_floating_point<_K>::value; 
    };
    
    template <typename _K>
    struct is_string {
        static constexpr bool value = std::is_same<std::string, _K>::value; 
    };
    
private:
    const std::string _M_name;
    const TYPE _M_type;
};

class kwarg_const : public kwarg {
public:
    ///{@
    kwarg_const(bool data, std::string name)
        : kwarg(name, kwarg::BOOL)
    { _M_data.boolean = data; }

    kwarg_const(int64_t data, std::string name)
        : kwarg(name, kwarg::INTEGRAL)
    { _M_data.integral = data; }

    kwarg_const(double data, std::string name)
        : kwarg(name, kwarg::FLOATING)
    { _M_data.floating = data; }

    kwarg_const(std::string data, std::string name)
        : kwarg(name, kwarg::STRING)
    { _M_data.str = data; }

    ///@}
    
    
    ///{@ 
    template <typename _Tp>
    typename std::enable_if<is_integral<_Tp>::value, _Tp>::type
    as() const {
        return static_cast<_Tp>(_M_data.integral);
    }

    template <typename _Tp>
    typename std::enable_if<is_bool<_Tp>::value, _Tp>::type
    as() const {
        return static_cast<_Tp>(_M_data.boolean);
    }

    template <typename _Tp>
    typename std::enable_if<is_floating<_Tp>::value, _Tp>::type
    as() const {
        return static_cast<_Tp>(_M_data.floating);
    }

    template <typename _Tp>
    typename std::enable_if<is_string<_Tp>::value, _Tp>::type
    as() const {
        return _M_data.str;
    }
    ///@}

private:
    /**
     */
    struct __kwarg_const_union {
        union {
            int64_t integral;
            double  floating;
            bool    boolean;
        };

        std::string str;
    } _M_data;
};


class kwarg_vector : public kwarg {
public:
    kwarg_vector(const std::string& name, std::vector<kwarg_const*>& source)
        : kwarg(name, kwarg::VECTOR), _M_vector(source)
    { }

    virtual ~kwarg_vector() {
        for (auto it = _M_vector.cbegin(); it != _M_vector.cend(); ++it) {
            delete ((*it));
        }
    }
    
    const std::vector<kwarg_const*>*
    operator->() const { 
        return &_M_vector;
    }

private:
    const std::vector<kwarg_const*> _M_vector;
};


//////////////////////////////////////////////////////////////////////////////////////////
// CONFIG SECTIONS
//////////////////////////////////////////////////////////////////////////////////////////

class config_section : public kwarg {

    template <typename _K>
    struct is_not_customized {
        static constexpr bool value = std::is_pointer<_K>::value;
    };

public:
    ///{@
    config_section& operator=(const config_section&) = delete;
    config_section(const config_section&) = delete;
    config_section(config_section&&) = delete;
    ///@}
    
    config_section* section(const std::string& name);
    config_section* object(const std::string& name) {
        return this->section(name);
    }

    kwarg_vector&
    vector(const std::string& key) const {
        return *static_cast<kwarg_vector*>(_M_get_kwarg(key));
    }
    
    ///{@
    /**
     */
    template <typename _Tp>
    _Tp
    get(const std::string& key) const {
        return static_cast<kwarg_const*>(_M_get_kwarg(key))->as<_Tp>();
    }
    
    /**
     */
    template <typename _Tp>
    _Tp
    get(const std::string& key, const _Tp& deflt) const {
        auto it = _M_kwargs.find(key);

        if (it == _M_kwargs.end())
            return deflt;
        else 
            return static_cast<kwarg_const*>(_M_get_kwarg(key))->as<_Tp>();
    }
    ///@}
    
    bool has_kwarg(const std::string& key) const;
    bool has_section(const std::string& key) const;

    void dump(int depth = 0);

protected:
    friend class config_parser;
    config_section(const std::string& name);
    virtual ~config_section();
    
    void _M_set_kwarg(kwarg* val);
    kwarg* _M_get_kwarg(const std::string& key) const;
   
    void _M_parse_file(const std::string& file_path, parse_trie<std::string>* regs);
    void _M_parse_iterator(_Iter& iter, parse_trie<std::string>* regs);
    void _M_parse_macro(_Iter& iter, parse_trie<std::string>* regs);
    kwarg* _M_parse_kwarg(std::string key, _Iter& iter, parse_trie<std::string>* regs);
    kwarg* _M_parse_vector(std::string key, _Iter& iter, parse_trie<std::string>* regs);
    
    void _M_parse_define(_Iter& iter, parse_trie<std::string>* regs);
    void _M_parse_export(_Iter& iter, parse_trie<std::string>* regs);
    void _M_parse_include(_Iter& iter, parse_trie<std::string>* regs);

private:
    struct umap_overload {
        size_t
        operator() (const std::string& key) const {
            return 1;
            size_t k = 2166136261;

            for (size_t i = 0; i < key.size(); i += 4)
                k = (k * 16777619) ^ key[i];

            return k;
        };
    };

    std::unordered_map<std::string, kwarg*, umap_overload> _M_kwargs;
    //std::unordered_map<std::string, kwarg*> _M_kwargs;
};

//////////////////////////////////////////////////////////////////////////////////////////
// CONFIG ROOT OBJECT
//////////////////////////////////////////////////////////////////////////////////////////

class config : public config_section {
public:
#if defined(CONFIG_SINGLETON)
    static constexpr bool has_singleton = true;
    static config* initialize(const std::string& file_path);
    static config* instance();
#else 
    static constexpr bool has_singleton = false;
    config(const std::string& file_path);
#endif 
    
    /**
     * Tests down a hierarchy against a casting type.  This function should be used to
     * ensure types are being parsed correctly.
     *
     * eg:
     *   config::instance()->section("s0")->section("s1")->get<float>("key");
     *   config::instance()->assert_type("s0.s1.key", kwarg::FLOATING);
     */
    bool assert_type(const std::string& key, kwarg::TYPE type) const;

private:
#if defined(CONFIG_SINGLETON)
    static config* _S_instance;
    config(const std::string& file_path);
#endif 

    parse_trie<std::string> _M_macro_regs;
};

#endif //__CONFIG_HH_
