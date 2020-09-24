#include <gtest/gtest.h>
#include <entt/entity/organizer.hpp>
#include <entt/entity/registry.hpp>

void t1(const int &, const double &) {}
void t2(const int &, char &, const double &) {}
void t3(int &) {}
void t4(int &, const char &) {}
void t5(const int &, double &) {}

#include <iostream>

TEST(Organizer, TODO) {
    entt::organizer organizer;

    // TODO
    organizer.emplace<&t1>("t1");
    organizer.emplace<&t2>("t2");
    organizer.emplace<&t3>("t3");
    organizer.emplace<&t4>("t4");
    organizer.emplace<&t5>("t5");

    for(auto &&task: organizer.top_level()) {
        std::cout << "[" << task.top_level() << "]" << task.name() << std::endl;
    }
}