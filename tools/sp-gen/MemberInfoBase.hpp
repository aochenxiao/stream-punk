#pragma once

#include <concepts>
#include <bitset>
#include <array>
#include <tuple>
#include <variant>
#include <string>

template <typename T, typename... Args>
inline constexpr bool type_in = (std::is_same_v<T, Args> || ...);

template <typename T, template <typename...> class X>
concept specialization_of = requires { [] <typename... Args>(X<Args...>) {}(std::declval<T>()); };

template <typename T, template <typename...> class... Xs>
concept specialization_of_any = (specialization_of<T, Xs> || ...);

template <typename T, template <typename...> class... Xs>
inline constexpr bool is_specialization_of_any_v = specialization_of_any<T, Xs...>;

template <typename T, template <typename...> class Template, typename... ExpectedFirstArgs>
struct has_first_template_args : std::false_type {};
template <template <typename...> class Template, typename... ExpectedFirstArgs, typename... RestArgs>
struct has_first_template_args<Template<ExpectedFirstArgs..., RestArgs...>, Template, ExpectedFirstArgs...> : std::true_type {};
template <typename T, template <typename...> class Template, typename... ExpectedFirstArgs>
constexpr bool has_first_template_args_v = has_first_template_args<T, Template, ExpectedFirstArgs...>::value;

template <typename T> struct is_std_bitset : std::false_type {};
template <std::size_t N> struct is_std_bitset<std::bitset<N>> : std::true_type {};
template <typename T> inline constexpr bool is_std_bitset_v = is_std_bitset<T>::value;

template <typename T> struct is_std_array : std::false_type {};
template <typename Elem, std::size_t Size>
struct is_std_array<std::array<Elem, Size>> : std::true_type {};
template <typename T>
inline constexpr bool is_std_array_v = is_std_array<std::remove_cv_t<T>>::value;

struct MemberInfo {
    std::string tname;
    std::string ivalue;
    std::string serializeCode;
    std::string deserializeCode;
    int arraySize = 0;
    std::string arrayElemType;
};

static const std::string VAL = "{{_}}";

static inline MemberInfo nullInfo { "", "", "", "" };

static inline void replaceAll(std::string& s, std::string_view from, std::string_view to) {
    size_t pos = 0;
    while ((pos = s.find(from, pos)) != std::string::npos) {
        s.replace(pos, from.size(), to);
        pos += to.size();
    }
}

template <typename T> struct type_sequence_extractor;
template <typename... Ts>
struct type_sequence_extractor<std::tuple<Ts...>> {
    using type = std::tuple<Ts...>;
    static constexpr size_t size = sizeof...(Ts);
    template <size_t I>
    using element = std::tuple_element_t<I, type>;
};
template <typename... Ts>
struct type_sequence_extractor<std::variant<Ts...>> {
    using type = std::variant<Ts...>;
    static constexpr size_t size = sizeof...(Ts);
    template <size_t I>
    using element = std::tuple_element_t<I, std::tuple<Ts...>>;
};