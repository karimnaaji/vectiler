#define CATCH_CONFIG_MAIN
#include "catch.hpp"
#define VECTILER_UNIT_TESTS
#include "vectiler.h"

TEST_CASE("", "") {
    Line line {{0.0, 0.0, 0.0}, {1.0, 1.0, 0.0}};
    subdivideLine(line, 4);
    for (auto p : line) {
        printf("{%f,%f,%f}\n",p.x,p.y,p.z);
    }
    REQUIRE(line.size() == 6);
    REQUIRE(line[0] == glm::vec3(0.0));
    REQUIRE(line[5] == glm::vec3(1.0, 1.0, 0.0));

    line = {{0.0, 0.0, 0.0}, {0.0, 1.0, 0.0}};
    subdivideLine(line, 4);

    for (auto p : line) {
        printf("{%f,%f,%f}\n",p.x,p.y,p.z);
    }
    REQUIRE(line.size() == 5);
    REQUIRE(line[0] == glm::vec3(0.0));
    REQUIRE(line[4] == glm::vec3(0.0, 1.0, 0.0));
}
