#include <gtest/gtest.h>
#include <entt/entity/organizer.hpp>
#include <entt/entity/registry.hpp>

void ro_int_rw_char_double(entt::view<entt::exclude_t<>, const int, char>, double &) {}
void ro_char_rw_int(entt::view<entt::exclude_t<>, int, const char>) {}
void ro_char_rw_double(entt::view<entt::exclude_t<>, const char>, double &) {}
void ro_int_double(entt::view<entt::exclude_t<>, const int>, const double &) {}

struct clazz {
	void ro_int_char_double(entt::view<entt::exclude_t<>, const int, const char>, const double &) {}
	void rw_int(entt::view<entt::exclude_t<>, int>) {}
	void rw_int_char(entt::view<entt::exclude_t<>, int, char>) {}
	void rw_int_char_double(entt::view<entt::exclude_t<>, int, char>, double &) {}
};

TEST(Organizer, EmplaceFreeFunction) {
	entt::organizer organizer;

	organizer.emplace<&ro_int_rw_char_double>("t1");
	organizer.emplace<&ro_char_rw_int>("t2");
	organizer.emplace<&ro_char_rw_double>("t3");
	organizer.emplace<&ro_int_double>("t4");

	const auto graph = organizer.graph();

	ASSERT_EQ(graph.size(), 4u);

	ASSERT_STREQ(graph[0].name(), "t1");
	ASSERT_STREQ(graph[1].name(), "t2");
	ASSERT_STREQ(graph[2].name(), "t3");
	ASSERT_STREQ(graph[3].name(), "t4");

	ASSERT_NE(graph[0].info(), graph[1].info());
	ASSERT_NE(graph[1].info(), graph[2].info());
	ASSERT_NE(graph[2].info(), graph[3].info());

	ASSERT_TRUE(graph[0u].top_level());
	ASSERT_FALSE(graph[1u].top_level());
	ASSERT_FALSE(graph[2u].top_level());
	ASSERT_FALSE(graph[3u].top_level());

	ASSERT_EQ(graph[0u].children().size(), 2u);
	ASSERT_EQ(graph[1u].children().size(), 1u);
	ASSERT_EQ(graph[2u].children().size(), 1u);
	ASSERT_EQ(graph[3u].children().size(), 0u);

	ASSERT_EQ(graph[0u].children()[0u], 1u);
	ASSERT_EQ(graph[0u].children()[1u], 2u);
	ASSERT_EQ(graph[1u].children()[0u], 3u);
	ASSERT_EQ(graph[2u].children()[0u], 3u);

	organizer.clear();

	ASSERT_EQ(organizer.graph().size(), 0u);
}

TEST(Organizer, EmplaceMemberFunction) {
	entt::organizer organizer;
	clazz instance;

	organizer.emplace<&clazz::ro_int_char_double>(instance, "t1");
	organizer.emplace<&clazz::rw_int>(instance, "t2");
	organizer.emplace<&clazz::rw_int_char>(instance, "t3");
	organizer.emplace<&clazz::rw_int_char_double>(instance, "t4");

	const auto graph = organizer.graph();

	ASSERT_EQ(graph.size(), 4u);

	ASSERT_STREQ(graph[0].name(), "t1");
	ASSERT_STREQ(graph[1].name(), "t2");
	ASSERT_STREQ(graph[2].name(), "t3");
	ASSERT_STREQ(graph[3].name(), "t4");

	ASSERT_NE(graph[0].info(), graph[1].info());
	ASSERT_NE(graph[1].info(), graph[2].info());
	ASSERT_NE(graph[2].info(), graph[3].info());

	ASSERT_TRUE(graph[0u].top_level());
	ASSERT_FALSE(graph[1u].top_level());
	ASSERT_FALSE(graph[2u].top_level());
	ASSERT_FALSE(graph[3u].top_level());

	ASSERT_EQ(graph[0u].children().size(), 1u);
	ASSERT_EQ(graph[1u].children().size(), 1u);
	ASSERT_EQ(graph[2u].children().size(), 1u);
	ASSERT_EQ(graph[3u].children().size(), 0u);

	ASSERT_EQ(graph[0u].children()[0u], 1u);
	ASSERT_EQ(graph[1u].children()[0u], 2u);
	ASSERT_EQ(graph[2u].children()[0u], 3u);

	organizer.clear();

	ASSERT_EQ(organizer.graph().size(), 0u);
}

TEST(Organizer, EmplaceDirectFunction) {
	entt::organizer organizer;
	clazz instance;

	// no aggressive comdat
	auto t1 = +[](const void *, entt::registry &registry) { registry.clear<int>(); };
	auto t2 = +[](const void *, entt::registry &registry) { registry.clear<char>(); };
	auto t3 = +[](const void *, entt::registry &registry) { registry.clear<double>(); };
	auto t4 = +[](const void *, entt::registry &registry) { registry.clear(); };

	organizer.emplace<const int>(t1, nullptr, "t1");
	organizer.emplace<int>(t2, &instance, "t2");
	organizer.emplace<int, char>(t3, nullptr, "t3");
	organizer.emplace<int, char, double>(t4, &instance, "t4");

	const auto graph = organizer.graph();

	ASSERT_EQ(graph.size(), 4u);

	ASSERT_STREQ(graph[0].name(), "t1");
	ASSERT_STREQ(graph[1].name(), "t2");
	ASSERT_STREQ(graph[2].name(), "t3");
	ASSERT_STREQ(graph[3].name(), "t4");

	ASSERT_TRUE(graph[0].callback() == t1);
	ASSERT_TRUE(graph[1].callback() == t2);
	ASSERT_TRUE(graph[2].callback() == t3);
	ASSERT_TRUE(graph[3].callback() == t4);

	ASSERT_EQ(graph[0].data(), nullptr);
	ASSERT_EQ(graph[1].data(), &instance);
	ASSERT_EQ(graph[2].data(), nullptr);
	ASSERT_EQ(graph[3].data(), &instance);

	ASSERT_EQ(graph[0].info(), entt::type_info{});
	ASSERT_EQ(graph[1].info(), entt::type_info{});
	ASSERT_EQ(graph[2].info(), entt::type_info{});

	ASSERT_TRUE(graph[0u].top_level());
	ASSERT_FALSE(graph[1u].top_level());
	ASSERT_FALSE(graph[2u].top_level());
	ASSERT_FALSE(graph[3u].top_level());

	ASSERT_EQ(graph[0u].children().size(), 1u);
	ASSERT_EQ(graph[1u].children().size(), 1u);
	ASSERT_EQ(graph[2u].children().size(), 1u);
	ASSERT_EQ(graph[3u].children().size(), 0u);

	ASSERT_EQ(graph[0u].children()[0u], 1u);
	ASSERT_EQ(graph[1u].children()[0u], 2u);
	ASSERT_EQ(graph[2u].children()[0u], 3u);

	organizer.clear();

	ASSERT_EQ(organizer.graph().size(), 0u);
}
