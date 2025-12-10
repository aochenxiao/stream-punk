#include <concepts>

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

template <typename T>
constexpr auto buildMemberInfoArray() {
    using Extractor = type_sequence_extractor<T>;
    constexpr size_t size = Extractor::size;

    return[] <size_t... Is> (std::index_sequence<Is...>) {
        return std::array{
            getMemberInfo<typename Extractor::template element<Is>>()...
        };
    }(std::make_index_sequence<size>{});
}
template <typename T>
constexpr auto getMemberInfoArray() { return buildMemberInfoArray<T>(); }
template <typename... Ts>
constexpr auto getMemberInfoArray(const std::tuple<Ts...>&) { return buildMemberInfoArray<std::tuple<Ts...>>(); }
template <typename... Ts>
constexpr auto getMemberInfoArray(const std::variant<Ts...>&) {
    return buildMemberInfoArray<std::variant<Ts...>>();
}

template <typename T>
struct is_std_array : std::false_type {};
template <typename Elem, std::size_t Size>
struct is_std_array<std::array<Elem, Size>> : std::true_type {};
template <typename T>
inline constexpr bool is_std_array_v = is_std_array<std::remove_cv_t<T>>::value;

struct MemberInfo {
    std::string tname;
    std::string ivalue;
    std::string serializeCode;
    std::string deserializeCode;
};

inline std::map<Sz, MemberInfo> typeToTs = {
    {E_type::u8     ,{"number" , "0"     , "o.write_u8($(value))", "i.read_u8()"}},
    {E_type::u16    ,{"number" , "0"     , "o.write_u16($(value))", "i.read_u16()"}},
    {E_type::u32    ,{"number" , "0"     , "o.write_u32($(value))", "i.read_u32()"}},
    {E_type::u64    ,{"bigint" , "0n"    , "o.write_u64($(value))", "i.read_u64()"}},
    {E_type::i8     ,{"number" , "0"     , "o.write_i8($(value))", "i.read_i8()"}},
    {E_type::i16    ,{"number" , "0"     , "o.write_i16($(value))", "i.read_i16()"}},
    {E_type::i32    ,{"number" , "0"     , "o.write_i32($(value))", "i.read_i32()"}},
    {E_type::i64    ,{"bigint" , "0n"    , "o.write_i64($(value))", "i.read_i64()"}},
    {E_type::f32    ,{"number" , "0.0"   , "o.write_f32($(value))", "i.read_f32()"}},
    {E_type::f64    ,{"number" , "0.0"   , "o.write_f64($(value))", "i.read_f64()"}},
    {E_type::bl     ,{"boolean","false"  , "o.write_bl($(value))", "i.read_bl()"}},
    {E_type::ch     ,{"string" ,"\"\""   , "o.write_ch($(value))", "i.read_ch()"}},
    //{E_type::chw    ,{"string" ,"\"\""   , "o.write_ch16($(value))", "i.read_ch16()"}},
    {E_type::ch8    ,{"string" ,"\"\""   , "o.write_ch8($(value) )", "i.read_ch8()"}},
    {E_type::ch16   ,{"string" ,"\"\""   , "o.write_ch16($(value))", "i.read_ch16()"}},
    {E_type::ch32   ,{"string" ,"\"\""   , "o.write_ch32($(value))", "i.read_ch32()"}},
};
inline MemberInfo nullInfo  { "", "", "", "" };
inline MemberInfo strInfo   { "string", "\"\"", "o.write_string($(value))", "i.read_string()" };
inline MemberInfo u8StrInfo { "string", "\"\"", "o.write_u8string($(value))", "i.read_u8string()" };
inline MemberInfo u16StrInfo{ "string", "\"\"", "o.write_u16string($(value))", "i.read_u16string()" };
inline MemberInfo u32StrInfo{ "Uint8Array", "\"\"", "o.write_u32string($(value))", "i.read_u32string()" };

