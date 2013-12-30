#ifndef __CONFIG_HH_
#define __CONFIG_HH_

#include <iterator>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <sstream>
#include <vector>

#include "config-bits.hh"

#if defined(CONFIG_SINGLETON)
#  define CFG config::instance()
#endif


//////////////////////////////////////////////////////////////////////////////////////////
// CONFIG EXCEPTION TYPES
//////////////////////////////////////////////////////////////////////////////////////////

/**
 * @class config_io_error
 * A file failed to be open & read.  The name of the offending file is stored in ::what()
 */
class config_io_error : public std::runtime_error {
public:
    config_io_error(std::string fname)
        : std::runtime_error(fname)
    {}
};

/**
 * @class config_key_error
 * Thrown if a key is accessed in a config_section which does not exist.
 */
class config_key_error : public std::runtime_error {
public:
    config_key_error(std::string key)
        : std::runtime_error(key)
    {}
};

/**
 * @class config_type_error
 * Thrown if a key is accessed in a config_section which does not exist.
 */
class config_type_error : public std::runtime_error {
public:
    config_type_error(std::string key)
        : std::runtime_error(key)
    {}
};


/**
 * @class config_parse_exception
 * Thrown if configuration syntax does not seem sane.  ::what() attemps to show reasonable
 * locality of the offending issue.
 */
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
/**
 * @class kwarg
 *
 * Every type in libconf is a subclass of `kwarg`.  A kwarg (and all subclass') are
 * non-movable.  The kwarg subclass holds nothing but the name and type of the super class
 * element.
 */
class kwarg {
public:
    enum TYPE { FLOATING, INTEGRAL, STRING, SECTION, UNDEFINED, VECTOR, BOOL };

    kwarg(const std::string& name, TYPE type)
        : _M_name(name), _M_type(type)
    {}

    virtual ~kwarg() {}

    std::string name() const
    { return _M_name; }

    TYPE type() const
    { return _M_type; }

    kwarg& operator=(const kwarg&) = delete;
    kwarg(const kwarg&) = delete;
    kwarg(kwarg&&) = delete;

protected:
    ///{@
    /**
     * These template macros are used inside std::enable_if<...> specializations as a
     * generic way of mapping cast types into the internal representation types.
     */
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
    ///@}

private:
    const std::string _M_name;
    const TYPE _M_type;
};

/**
 * @class kwarg_const
 * Represents access to any primitive data type.
 */
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
    /**
     * These accessor functions are needed due to the local union.  It must first access
     * the proper union element and then perform a proper cast.
     */
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
     * note:  .str element was once in the union via C++11 unrestricted union.  This was
     * deemed not worth the effort of managing the potential memory leaks inside the
     * destructor.
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

/**
 * A kwarg vector has no implemented functions of its own.  Rather, it exposes the
 * internal vector<kwarg_const*> object via the -> operator.  As a consequence it is
 * generally as fast as the equivalent vector<_Tp>
 */
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
/**
 * @class config_section
 *
 * Represents the "object" element of the config library in that it internally maps
 * key->value pairs.
 */
class config_section : public kwarg {
public:
    using map_type       = std::map<std::string, kwarg*>;
    using const_iterator = map_type::const_iterator;

    ///{@
    /**
     * these functions access internal kwarg* elements and cast them to the appropriate
     * type /with/ typechecking.
     *
     * @throw config_key_error
     * @throw config_type_error
     */
    config_section* section(const std::string& name) const;
    config_section* object(const std::string& name) const { return this->section(name); }
    kwarg_vector& vector(const std::string& key) const {
        return *static_cast<kwarg_vector*>(_M_get_kwarg(key));
    }
    ///@}

    ///{@
    const_iterator cbegin() const;
    const_iterator cend() const;
    ///@}

    ///{@
    /**
     * @template _Tp should be a primitive
     * @throw config_key_error
     */
    template <typename _Tp>
    _Tp
    get(const std::string& key) const {
        return static_cast<kwarg_const*>(_M_get_kwarg(key))->as<_Tp>();
    }

    /**
     * Like @see config_section::get(string) except if the value cannot be found it
     * returns the value passed to deflt instead of excepting.
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

    ///{@
    /**
     * element checks.  if the has_{type} implies a specific kwarg type it will return
     * false if an identical key with a different type exists.
     */
    bool has_kwarg(const std::string& key) const;
    bool has_section(const std::string& key) const;
    bool has_vector(const std::string& key) const;
    ///@}

    /// do not rely on this function : simply prints data out to stderr
    void dump(int depth = 0);

protected:
    config_section(const std::string& name);
    virtual ~config_section();

    ///{@
    /**
     * ::_M_get_kwarg(...) functions @throw config_key_error
     */
    void _M_set_kwarg(kwarg* val);
    kwarg* _M_get_kwarg(const std::string& key) const;
    kwarg* _M_get_kwarg(const std::string& key, kwarg::TYPE t) const;
    ///@}

    ///{@
    void _M_parse_file(const std::string& file_path, parse_trie<std::string>* regs);
    void _M_parse_iterator(_Iter& iter, parse_trie<std::string>* regs);
    ///@}

    ///{@
    void _M_parse_macro(_Iter& iter, parse_trie<std::string>* regs);
    void _M_parse_define(_Iter& iter, parse_trie<std::string>* regs);
    void _M_parse_import(_Iter& iter, parse_trie<std::string>* regs);
    void _M_parse_include(_Iter& iter, parse_trie<std::string>* regs);
    ///@}

    ///{@
    kwarg* _M_parse_kwarg(std::string key, _Iter& iter, parse_trie<std::string>* regs);
    kwarg* _M_parse_vector(std::string key, _Iter& iter, parse_trie<std::string>* regs);
    ///@}

private:
    map_type _M_kwargs;
};

//////////////////////////////////////////////////////////////////////////////////////////
// CONFIG ROOT OBJECT
//////////////////////////////////////////////////////////////////////////////////////////
/**
 * @class config
 * The config class is a specialized config_section identifying the 'root' of the config
 * hierarchy.
 *
 * It has two compile modes
 * #ifdef CONFIG_SINGLETON
 *  A singlenton instance can be initialized and accessed via public static methods and
 *  only a single config can exist in the applicatin space.  Memory is managed by the
 *  config class.  In this case it registers and ::atexit(...) function which frees memory
 *  appropriately.
 *
 * #ifndef CONFIG_SINGLETON
 *  Any number of configuration objects can be initialized, however memory is managed by
 *  the creator.
 */
class config : public config_section {
public:
#if defined(CONFIG_SINGLETON)
    static constexpr bool has_singleton = true;
    static config* initialize(const std::string& file_path);
    static config* instance();
#else
    static constexpr bool has_singleton = false;
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

#if defined(CONFIG_SINGLETON)
private:
    static config* _S_instance;
#endif
    /**
     * @WARNING: constructor can throw exceptions.
     *
     * @throw config_parse_exception
     */
    config(const std::string& file_path);

private:
    parse_trie<std::string> _M_macro_regs;
};

#endif //__CONFIG_HH_
