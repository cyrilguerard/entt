#ifndef ENTT_ENTITY_ORGANIZER_HPP
#define ENTT_ENTITY_ORGANIZER_HPP


#include <cstddef>
#include <algorithm>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>
#include "../core/type_info.hpp"
#include "../core/type_traits.hpp"
#include "fwd.hpp"
#include "helper.hpp"


namespace entt {


namespace internal {


template<typename>
struct is_view: std::false_type {};

template<typename Entity, typename... Exclude, typename... Component>
struct is_view<basic_view<Entity, exclude_t<Exclude...>, Component...>>: std::true_type {};

template<typename Type>
inline constexpr bool is_view_v = is_view<Type>::value;


template<typename Type, typename Override>
struct unpack_type {
    using ro = std::conditional_t<
        type_list_contains_v<Override, std::add_const_t<Type>> || (std::is_const_v<Type> && !type_list_contains_v<Override, std::remove_const_t<Type>>),
        type_list<std::remove_const_t<Type>>,
        type_list<>
    >;

    using rw = std::conditional_t<
        type_list_contains_v<Override, std::remove_const_t<Type>> || (!std::is_const_v<Type> && !type_list_contains_v<Override, std::add_const_t<Type>>),
        type_list<Type>,
        type_list<>
    >;
};

template<typename Entity, typename... Override>
struct unpack_type<basic_registry<Entity>, type_list<Override...>> {
    using ro = type_list<>;
    using rw = type_list<>;
};

template<typename Entity, typename... Override>
struct unpack_type<const basic_registry<Entity>, type_list<Override...>>
    : unpack_type<basic_registry<Entity>, type_list<Override...>>
{};

template<typename Entity, typename... Exclude, typename... Component, typename... Override>
struct unpack_type<basic_view<Entity, exclude_t<Exclude...>, Component...>, type_list<Override...>> {
    using ro = type_list_cat_t<type_list<Exclude...>, typename unpack_type<Component, type_list<Override...>>::ro...>;
    using rw = type_list_cat_t<typename unpack_type<Component, type_list<Override...>>::rw...>;
};

template<typename Entity, typename... Exclude, typename... Component, typename... Override>
struct unpack_type<const basic_view<Entity, exclude_t<Exclude...>, Component...>, type_list<Override...>>
    : unpack_type<basic_view<Entity, exclude_t<Exclude...>, Component...>, type_list<Override...>>
{};


template<typename, typename>
struct resource;

template<typename... Args, typename... Req>
struct resource<type_list<Args...>, type_list<Req...>> {
    using args = type_list<std::remove_const_t<Args>...>;
    using ro = type_list_cat_t<typename unpack_type<Args, type_list<Req...>>::ro..., typename unpack_type<Req, type_list<>>::ro...>;
    using rw = type_list_cat_t<typename unpack_type<Args, type_list<Req...>>::rw..., typename unpack_type<Req, type_list<>>::rw...>;
};


template<typename... Req, typename Ret, typename... Args>
resource<type_list<std::remove_reference_t<Args>...>, type_list<Req...>> to_resource(Ret(*)(Args...));

template<typename... Req, typename Ret, typename Class, typename... Args>
resource<type_list<std::remove_reference_t<Args>...>, type_list<Req...>> to_resource(Ret(Class:: *)(Args...));

template<typename... Req, typename Ret, typename Class, typename... Args>
resource<type_list<std::remove_reference_t<Args>...>, type_list<Req...>> to_resource(Ret(Class:: *)(Args...) const);

template<typename... Req>
resource<type_list<>, type_list<Req...>> to_resource();


}


template<typename Entity>
class basic_organizer final {
    using callback_type = void(const void *, entt::basic_registry<Entity> &);
    using prepare_type = void(entt::basic_registry<Entity> &);
    using dependency_type = std::size_t(const bool, type_info *, const std::size_t);

    struct vertex_data final {
        std::size_t ro_count{};
        std::size_t rw_count{};
        const char *name{};
        const void *payload{};
        callback_type *callback{};
        dependency_type *dependency;
        prepare_type *prepare{};
        type_info info{};
    };

    template<typename Type>
    [[nodiscard]] static decltype(auto) extract(basic_registry<Entity> &reg) {
        if constexpr(std::is_same_v<Type, basic_registry<Entity>>) {
            return reg;
        } else if constexpr(internal::is_view_v<Type>) {
            return as_view{reg};
        } else {
            return reg.template ctx_or_set<std::remove_reference_t<Type>>();
        }
    }

    template<typename... Args>
    [[nodiscard]] static auto to_args(basic_registry<Entity> &reg, type_list<Args...>) {
        return std::forward_as_tuple(extract<Args>(reg)...);
    }

    template<typename... Type>
    static std::size_t fill_dependencies(type_list<Type...>, type_info *buffer, const std::size_t count) {
        if constexpr(sizeof...(Type) == 0u) {
            return {};
        } else {
            type_info info[sizeof...(Type)]{type_id<Type>()...};
            const auto length = std::min(count, sizeof...(Type));
            std::copy_n(info, length, buffer);
            return length;
        }
    }

    template<typename... RO, typename... RW>
    void track_dependencies(std::size_t index, const bool requires_registry, type_list<RO...>, type_list<RW...>) {
        dependencies[type_hash<basic_registry<Entity>>::value()].emplace_back(index, requires_registry && (sizeof...(RO) + sizeof...(RW) == 0u));
        (dependencies[type_hash<RO>::value()].emplace_back(index, false), ...);
        (dependencies[type_hash<RW>::value()].emplace_back(index, true), ...);
    }

    [[nodiscard]] std::vector<bool> adjacency_matrix() {
        const auto length = vertices.size();
        std::vector<bool> edges(length * length, false);

        // creates the ajacency matrix
        for(const auto &deps: dependencies) {
            const auto last = deps.second.cend();
            auto it = deps.second.cbegin();

            while(it != last) {
                if(it->second) {
                    // rw item
                    if(auto curr = it++; it != last) {
                        if(it->second) {
                            edges[curr->first * length + it->first] = true;
                        } else {
                            if(const auto next = std::find_if(it, last, [](const auto &elem) { return elem.second; }); next != last) {
                                for(; it != next; ++it) {
                                    edges[curr->first * length + it->first] = true;
                                    edges[it->first * length + next->first] = true;
                                }
                            } else {
                                for(; it != next; ++it) {
                                    edges[curr->first * length + it->first] = true;
                                }
                            }
                        }
                    }
                } else {
                    // ro item, possibly only on first iteration
                    if(const auto next = std::find_if(it, last, [](const auto &elem) { return elem.second; }); next != last) {
                        for(; it != next; ++it) {
                            edges[it->first * length + next->first] = true;
                        }
                    } else {
                        it = last;
                    }
                }
            }
        }

        // computes the transitive closure
        for(std::size_t vk{}; vk < length; ++vk) {
            for(std::size_t vi{}; vi < length; ++vi) {
                for(std::size_t vj{}; vj < length; ++vj) {
                    edges[vi * length + vj] = edges[vi * length + vj] || (edges[vi * length + vk] && edges[vk * length + vj]);
                }
            }
        }

        // applies the transitive reduction
        for(std::size_t vert{}; vert < length; ++vert) {
            edges[vert * length + vert] = false;
        }

        for(std::size_t vj{}; vj < length; ++vj) {
            for(std::size_t vi{}; vi < length; ++vi) {
                if(edges[vi * length + vj]) {
                    for(std::size_t vk{}; vk < length; ++vk) {
                        if(edges[vj * length + vk]) {
                            edges[vi * length + vk] = false;
                        }
                    }
                }
            }
        }

        return edges;
    }

public:
    using entity_type = Entity;
    using size_type = std::size_t;
    using function_type = callback_type;

    struct vertex {
        vertex(const bool vtype, vertex_data data, std::vector<std::size_t> edges)
            : is_top_level{vtype},
              node{std::move(data)},
              reachable{std::move(edges)}
        {}

        size_type ro_dependency(type_info *buffer, const std::size_t length) const{
            return node.dependency(false, buffer, length);
        }

        size_type rw_dependency(type_info *buffer, const std::size_t length) const {
            return node.dependency(true, buffer, length);
        }

        size_type ro_count() const ENTT_NOEXCEPT {
            return node.ro_count;
        }

        size_type rw_count() const ENTT_NOEXCEPT {
            return node.rw_count;
        }

        bool top_level() const ENTT_NOEXCEPT {
            return is_top_level;
        }

        type_info info() const ENTT_NOEXCEPT {
            return node.info;
        }

        const char * name() const ENTT_NOEXCEPT {
            return node.name;
        }

        function_type * callback() const ENTT_NOEXCEPT {
            return node.callback;
        }

        const void * data() const ENTT_NOEXCEPT {
            return node.payload;
        }

        void prepare(basic_registry<entity_type> &reg) const {
            node.prepare(reg);
        }

        const std::vector<std::size_t> & children() const ENTT_NOEXCEPT {
            return reachable;
        }

    private:
        bool is_top_level;
        vertex_data node;
        std::vector<std::size_t> reachable;
    };

    template<auto Candidate, typename... Req>
    void emplace(const char *name = nullptr) {
        using resource_type = decltype(internal::to_resource<Req...>(Candidate));
        constexpr auto requires_registry = type_list_contains_v<typename resource_type::args, basic_registry<entity_type>>;
        callback_type *callback = +[](const void *, basic_registry<entity_type> &reg) { std::apply(Candidate, to_args(reg, typename resource_type::args{})); };

        track_dependencies(vertices.size(), requires_registry, typename resource_type::ro{}, typename resource_type::rw{});

        vertices.push_back({
            type_list_size_v<typename resource_type::ro>,
            type_list_size_v<typename resource_type::rw>,
            name,
            nullptr,
            callback,
            +[](const bool rw, type_info *buffer, const std::size_t length) { return rw ? fill_dependencies(typename resource_type::rw{}, buffer, length) : fill_dependencies(typename resource_type::ro{}, buffer, length); },
            +[](basic_registry<entity_type> &reg) { void(to_args(reg, typename resource_type::args{})); },
            type_id<integral_constant<Candidate>>()
        });
    }

    template<auto Candidate, typename... Req, typename Type>
    void emplace(Type &value_or_instance, const char *name = nullptr) {
        using resource_type = decltype(internal::to_resource<Req...>(Candidate));
        constexpr auto requires_registry = type_list_contains_v<typename resource_type::args, basic_registry<entity_type>>;

        callback_type *callback = +[](const void *payload, basic_registry<entity_type> &reg) {
            Type *curr = static_cast<Type *>(const_cast<std::conditional_t<std::is_const_v<Type>, const void *, void *>>(payload));
            std::apply(Candidate, std::tuple_cat(std::forward_as_tuple(*curr), to_args(reg, typename resource_type::args{})));
        };

        track_dependencies(vertices.size(), requires_registry, typename resource_type::ro{}, typename resource_type::rw{});

        vertices.push_back({
            type_list_size_v<typename resource_type::ro>,
            type_list_size_v<typename resource_type::rw>,
            name,
            &value_or_instance,
            callback,
            +[](const bool rw, type_info *buffer, const std::size_t length) { return rw ? fill_dependencies(typename resource_type::rw{}, buffer, length) : fill_dependencies(typename resource_type::ro{}, buffer, length); },
            +[](basic_registry<entity_type> &reg) { void(to_args(reg, typename resource_type::args{})); },
            type_id<integral_constant<Candidate>>()
        });
    }

    template<typename... Req>
    void emplace(function_type *func, const void *data = nullptr, const char *name = nullptr) {
        using resource_type = decltype(internal::to_resource<Req...>());
        track_dependencies(vertices.size(), true, typename resource_type::ro{}, typename resource_type::rw{});

        vertices.push_back({
            type_list_size_v<typename resource_type::ro>,
            type_list_size_v<typename resource_type::rw>,
            name,
            data,
            func,
            +[](const bool rw, type_info *buffer, const std::size_t length) { return rw ? fill_dependencies(typename resource_type::rw{}, buffer, length) : fill_dependencies(typename resource_type::ro{}, buffer, length); },
            +[](basic_registry<entity_type> &reg) { void(to_args(reg, typename resource_type::args{})); },
            type_info{}
        });
    }

    std::vector<vertex> graph() {
        const auto edges = adjacency_matrix();

        // creates the adjacency list
        std::vector<vertex> adjacency_list{};
        adjacency_list.reserve(vertices.size());

        for(std::size_t col{}, length = vertices.size(); col < length; ++col) {
            std::vector<std::size_t> reachable{};
            const auto row = col * length;
            bool is_top_level = true;

            for(std::size_t next{}; next < length; ++next) {
                if(edges[row + next]) {
                    reachable.push_back(next);
                }
            }

            for(std::size_t next{}; next < length && is_top_level; ++next) {
                is_top_level = !edges[next * length + col];
            }

            adjacency_list.emplace_back(is_top_level, vertices[col], std::move(reachable));
        }

        return adjacency_list;
    }

    void clear() {
        dependencies.clear();
        vertices.clear();
    }

private:
    std::unordered_map<entt::id_type, std::vector<std::pair<std::size_t, bool>>> dependencies;
    std::vector<vertex_data> vertices;
};


}


#endif
