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

namespace ini_config {

/**
 * A minimal container for a given string literal, used to allow string
 * literals to be passed as template parameters.
 */
template<typename T = char, unsigned long int N = 0>
struct string_container {
    using char_type = T;

    char_type data[N];
    constexpr string_container(const char_type (&s)[N]) noexcept {
        auto dst = data;
        for (auto src = s; src != s + N; ++src)
            *dst++ = *src;
    }
    constexpr operator const char_type *() const noexcept {
        return data;
    }
    constexpr auto size() const noexcept {
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
                } while (*++p != ']');
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
                // At the end
                if (m_current.first != nullptr)
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

        // Parameter is a location within kvp_buffer
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
            return m_pos <=> other.m_pos;
        }
        constexpr bool operator==(const iterator& other) const noexcept {
            return m_pos == other.m_pos && m_current.first == other.m_current.first;
        }
    };

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
    /**
     * Returns beginning iterator for the given section.
     */
    constexpr auto begin(const char_type *section) const noexcept {
        for (auto it = begin(); it != end(); ++it) {
            if (it->section != nullptr && stringmatch(it->section, section))
                return it;
        }
        return end();
    }
    constexpr auto end() const noexcept {
        return iterator(kvp_buffer + sizeof(kvp_buffer) - 1);
    }
    /**
     * Returns end iterator for the given section.
     */
    constexpr auto end(const char_type *section) const noexcept {
        for (auto it = begin(); it != end(); ++it) {
            if (it->section != nullptr && stringmatch(it->section, section)) {
                for (++it; it != end() && stringmatch(it->section, section); ++it);
                return it;
            }
        }
        return end();
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
     * Creates a 'view' for the given section (use with ranged for).
     */
    constexpr auto section(const char_type *s) const noexcept {
        return section_view(*this, s);
    }

    /**
     * Finds and returns the pair with the given key.
     * Returns an empty string on failure.
     */
    constexpr auto get(const char_type *key) const noexcept {
        for (auto kvp : *this) {
            if (stringmatch(kvp.first, key))
                return kvp.second;
        }
        return "";
    }
    /**
     * Finds and returns the pair with the given key, in the given section.
     * Returns an empty string on failure.
     */
    constexpr auto get(const char_type *sec, const char_type *key) const noexcept {
        for (auto kvp : section(sec)) {
            if (stringmatch(kvp.first, key))
                return kvp.second;
        }
        return "";
    }
    /**
     * Array-style access to values. Searches all sections.
     * Returns an empty string on failure.
     */
    constexpr auto operator[](const char_type *key) const noexcept {
        return get(key);
    }
};

} // namespace ini_config

/**
 * _ini suffix definition.
 */
template <ini_config::string_container Input>
constexpr auto operator ""_ini()
{
    return ini_config::ini_config<Input>();
}

#endif // TCSULLIVAN_INI_CONFIG_HPP