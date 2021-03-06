/**
 * ini_config.hpp - Evaluate INI configs at compile-time for efficient use at run-time.
 * Written by Clyne Sullivan.
 * https://github.com/tcsullivan/ini-config
 */

#ifndef TCSULLIVAN_INI_CONFIG_HPP
#define TCSULLIVAN_INI_CONFIG_HPP

// Uncomment below to run std::forward_iterator check
//#define TCSULLIVAN_INI_CONFIG_CHECK_FORWARD_ITERATOR

#ifdef TCSULLIVAN_INI_CONFIG_CHECK_FORWARD_ITERATOR
#include <iterator> // std::forward_iterator
#endif

#include <concepts> // std::integral, std::floating_point
#include <compare> // std::strong_ordering

namespace ini_config {

/**
 * A minimal container for a given string literal, used to allow string
 * literals to be passed as template parameters.
 */
template<typename T = char, unsigned long int N = 0>
struct string_container {
    using char_type = T;

    char_type data[N];
    consteval string_container(const char_type (&s)[N]) noexcept {
        auto dst = data;
        for (auto src = s; src != s + N; ++src)
            *dst++ = *src;
    }
    consteval operator const char_type *() const noexcept {
        return data;
    }
    consteval auto size() const noexcept {
        return N;
    }
    consteval auto begin() const noexcept {
        return data;
    }
    consteval auto end() const noexcept {
        return data + N;
    }
};

template<auto Input>
class ini_config
{
    // Private implementation stuff must be defined first.
    // Jump to the public section below for the available interface.

    using char_type = typename decltype(Input)::char_type;

    consteval static bool isgraph(char_type c) noexcept {
        return c > ' ' && c != 0x7F;
    }
    consteval static bool iseol(char_type c) noexcept {
        return c == '\n' || c == '\0';
    }
    consteval static bool iscomment(char_type c) noexcept {
        return c == ';' || c == '#';
    }

    constexpr static bool stringmatch(const char_type *a, const char_type *b) noexcept {
        while (*a == *b && (*a != '\0' || *b != '\0'))
            ++a, ++b;
        return *a == *b && *a == '\0';
    }
    consteval static const char_type *nextline(const char_type *in) noexcept {
        while (!iseol(*in))
            ++in;
        return in + 1;
    }

    template<std::integral int_type>
    constexpr static int_type from_string(const char_type *str) noexcept {
        int_type ret = 0;
        bool neg = *str == '-';
        if (neg)
            ++str;
        for (; *str && *str >= '0' && *str <= '9'; ++str)
            ret = ret * 10 + (*str - '0');
        return !neg ? ret : -ret;
    }
    template<std::floating_point float_type>
    constexpr static float_type from_string(const char_type *str) noexcept {
        float_type ret = 0;
        bool neg = *str == '-';
        if (neg)
            ++str;
        for (; *str && *str >= '0' && *str <= '9'; ++str)
            ret = ret * 10 + (*str - '0');
        if (*str == '.') {
            float_type dec = 0.1;
            for (++str; *str && *str >= '0' && *str <= '9'; ++str) {
                ret += (*str - '0') * dec;
                dec /= 10;
            }
        }
        return !neg ? ret : -ret;
    }

    // Validates INI syntax, returning the count of chars
    // needed to store all section names, keys, and values
    consteval static unsigned int verify_and_size() {
        unsigned int sizechars = 0;

        auto line = Input.begin();
        do {
            auto p = line;
            // Remove beginning whitespace
            for (; !iseol(*p) && !isgraph(*p); ++p);

            if (iseol(*p) || iscomment(*p))
                continue;

            // Check for section header
            if (*p == '[') {
                do {
                    ++sizechars;
                    ++p;
                } while (!iseol(*p) && *p != ']');
                if (iseol(*p))
                    throw "Bad section tag!";
                ++sizechars; // Null terminator
                continue;
            }

            // This is the key (and whitespace until =)
            for (bool keyend = false; !iseol(*p); ++p) {
                if (*p == '=')
                    break;
                if (!keyend) {
                    if (!isgraph(*p))
                        keyend = true;
                    else
                        ++sizechars;
                } else {                    
                    if (isgraph(*p))
                        throw "Invalid key!";
                }   
            }
            if (*p != '=')
                throw "Invalid key!";
            ++p;

            // Next is the value
            auto oldsizechars = sizechars;
            for (bool valuestart = false; !iseol(*p); ++p) {
                if (!valuestart && isgraph(*p))
                    valuestart = true;
                if (valuestart)
                    ++sizechars;
            }
            if (oldsizechars == sizechars)
                throw "No value!";

            // All good, add two bytes for key/value terminators
            sizechars += 2;
        } while ((line = nextline(line)) != Input.end());
        
        return sizechars;
    }

    // Counts how many key-value pairs are in the text
    consteval static unsigned int kvpcount() noexcept {
        unsigned int count = 0;

        auto line = Input.begin();
        do {
            auto p = line;
            // Remove beginning whitespace
            for (; !iseol(*p) && !isgraph(*p); ++p);

            if (iseol(*p) || iscomment(*p) || *p == '[')
                continue;

            count++; // Must be a key-value pair
        } while ((line = nextline(line)) != Input.end());
        
        return count;
    }

    // A compact buffer for section names, keys, and values
    char_type kvp_buffer[verify_and_size() + 1] = {};

    consteval void fill_kvp_buffer() noexcept {
        auto bptr = kvp_buffer;

        auto line = Input.begin();
        do {
            auto p = line;
            // Remove beginning whitespace
            for (; !iseol(*p) && !isgraph(*p); ++p);

            if (iseol(*p) || iscomment(*p))
                continue;

            if (*p == '[') {
                do {
                    *bptr++ = *p++;
                } while (*p != ']');
                *bptr++ = '\0';
                continue;
            }

            // This is the key (and whitespace until =)
            for (bool keyend = false; !iseol(*p); ++p) {
                if (*p == '=')
                    break;
                if (!keyend) {
                    if (!isgraph(*p))
                        keyend = true;
                    else
                        *bptr++ = *p;
                } 
            }
            ++p;
            *bptr++ = '\0';

            // Next is the value
            for (bool valuestart = false; !iseol(*p); ++p) {
                if (!valuestart && isgraph(*p))
                    valuestart = true;
                if (valuestart)
                    *bptr++ = *p;
            }
            *bptr++ = '\0';
        } while ((line = nextline(line)) != Input.end());
        *bptr = '\0';
    }

public:
    // Stores a key-value pair, including a section identifier
    struct kvp {
        const char_type *section = nullptr;
        const char_type *first = nullptr;
        const char_type *second = nullptr;
    };

    class iterator {
        const char_type *m_pos = nullptr;
        kvp m_current = {};

        constexpr const auto& get_next() noexcept {
            if (*m_pos == '\0') {
                // Set first to nullptr to indicate that we're at the end
                m_current.first = nullptr;
            } else {
                // Enter new section(s) if necessary
                while (*m_pos == '[') {
                    m_current.section = m_pos + 1;
                    while (*m_pos++ != '\0');
                }
                m_current.first = m_pos;
                while (*m_pos++ != '\0');
                m_current.second = m_pos;
                while (*m_pos++ != '\0');
            }
            return m_current;
        }

    public:
        using difference_type = long int;
        using value_type = kvp;

        // 'pos' is a location within kvp_buffer
        constexpr iterator(const char_type *pos) noexcept
            : m_pos(pos)
        {
            while (*m_pos == '[') {
                m_current.section = m_pos + 1;
                while (*m_pos++ != '\0');
            }
            get_next();
        }
        constexpr iterator() = default;

        constexpr const auto& operator*() const noexcept {
            return m_current;
        }
        constexpr const auto *operator->() const noexcept {
            return &m_current;
        }
        constexpr auto& operator++() noexcept {
            get_next();
            return *this;
        }
        constexpr auto operator++(int) noexcept {
            auto copy = *this;
            get_next();
            return copy;
        }
        constexpr auto operator<=>(const iterator& other) const noexcept {
            if (auto poscomp = m_pos <=> other.m_pos; poscomp != 0)
                return poscomp;
            if (m_current.first == nullptr)
                return std::strong_ordering::greater;
            else if (other.m_current.first == nullptr)
                return std::strong_ordering::less;
            else
                return m_current.first <=> other.m_current.first;
        }
        constexpr bool operator==(const iterator& other) const noexcept {
            return m_pos == other.m_pos && m_current.first == other.m_current.first;
        }
    };

    /**
     * Constructs the ini_config object, populating the section/key/value
     * buffer.
     */
    consteval ini_config() noexcept
#ifdef TCSULLIVAN_INI_CONFIG_CHECK_FORWARD_ITERATOR
        requires(std::forward_iterator<iterator>)
#endif
    {
        fill_kvp_buffer();
    }

    /**
     * Returns the number of key-value pairs.
     */
    consteval auto size() const noexcept {
        return kvpcount();
    }

    constexpr auto begin() const noexcept {
        return iterator(kvp_buffer);
    }
    constexpr auto end() const noexcept {
        return iterator(kvp_buffer + sizeof(kvp_buffer) / sizeof(char_type) - 1);
    }
    constexpr auto cbegin() const noexcept {
        return begin();
    }
    constexpr auto cend() const noexcept {
        return end();
    }

    /**
     * Returns beginning iterator for the given section.
     */
    constexpr auto begin(const char_type *section) const noexcept {
        auto it = begin();
        do {
            if (it->section != nullptr && stringmatch(it->section, section))
                break;
        } while (++it != end());
        return it;
    }
    
    /** 
     * Returns end iterator for the given section.
     */
    constexpr auto end(const char_type *section) const noexcept {
        auto it = begin(section);
        while (++it != end() && stringmatch(it->section, section));
        return it;
    }

    class section_view {
        iterator m_begin;
        iterator m_end;
    public:
        constexpr section_view(const ini_config& ini, const char_type *section)
            : m_begin(ini.begin(section)), m_end(ini.end(section)) {}
        constexpr auto begin() const noexcept {
            return m_begin;
        }
        constexpr auto end() const noexcept {
            return m_end;
        }
    };

    /**
     * Creates a 'view' for the given section, for use with ranged for.
     */
    constexpr auto section(const char_type *s) const noexcept {
        return section_view(*this, s);
    }

    /**
     * Returns the value for the given key as a string.
     * Returns "" if key does not exist.
     */
    consteval auto get(const char_type *key) const noexcept {
        for (auto kvp : *this) {
            if (stringmatch(kvp.first, key))
                return kvp.second;
        }
        return "";
    }
    /**
     * Returns the value for the given key, converted to the given type.
     * Returns zero if key does not exist.
     */
    template<typename T> requires(std::integral<T> || std::floating_point<T>)
    consteval T get(const char_type *key) const noexcept {
        return from_string<T>(get(key));
    }
    /**
     * Returns the value for the given key in the given section.
     * Returns "" on failure.
     */
    consteval auto get(const char_type *sec, const char_type *key) const noexcept {
        for (auto kvp : section(sec)) {
            if (stringmatch(kvp.first, key))
                return kvp.second;
        }
        return "";
    }
    /**
     * Returns the value for the given key in the given section,
     * converting it to the specified type.
     * Returns zero on failure.
     */
    template<typename T> requires(std::integral<T> || std::floating_point<T>)
    consteval T get(const char_type *sec, const char_type *key) const noexcept {
        return from_string<T>(get(sec, key));
    }

    /**
     * tryget() calls are for run-time use when 'sec' or 'key'
     * is not known at compile-time.
     */
    auto tryget(const char_type *key) const noexcept {
        for (auto kvp : *this) {
            if (stringmatch(kvp.first, key))
                return kvp.second;
        }
        return "";
    }
    template<typename T> requires(std::integral<T> || std::floating_point<T>)
    T tryget(const char_type *key) const noexcept {
        return from_string<T>(tryget(key));
    }
    auto tryget(const char_type *sec, const char_type *key) const noexcept {
        for (auto kvp : section(sec)) {
            if (stringmatch(kvp.first, key))
                return kvp.second;
        }
        return "";
    }
    template<typename T> requires(std::integral<T> || std::floating_point<T>)
    T tryget(const char_type *sec, const char_type *key) const noexcept {
        return from_string<T>(tryget(sec, key));
    }

    consteval bool contains(const char_type *key) const noexcept {
        return *get(key) != '\0';
    }
    consteval bool contains(const char_type *sec, const char_type *key) const noexcept {
        return *get(sec, key) != '\0';
    }
    bool trycontains(const char_type *key) const noexcept {
        return *tryget(key) != '\0';
    }
    bool trycontains(const char_type *sec, const char_type *key) const noexcept {
        return *tryget(sec, key) != '\0';
    }
};

} // namespace ini_config

/**
 * _ini suffix definition.
 */
template <ini_config::string_container Input>
consteval auto operator ""_ini()
{
    return ini_config::ini_config<Input>();
}

// For MSVC, the below alternative seems promising, though
// MSVC v19.28 complains about running out of heap.
template <ini_config::string_container Input>
constexpr auto make_ini_config = ini_config::ini_config<Input>();

#endif // TCSULLIVAN_INI_CONFIG_HPP

