#include <catch2/catch_test_macros.hpp>

#include <watpocket/watpocket.hpp>

TEST_CASE("parse_residue_selectors parses residue-only selectors", "[api]")
{
  const auto selectors = watpocket::parse_residue_selectors("12,15,18");
  REQUIRE(selectors.size() == 3);
  REQUIRE_FALSE(selectors[0].chain.has_value());
  REQUIRE(selectors[0].resid == 12);
  REQUIRE(selectors[2].resid == 18);
}

TEST_CASE("parse_residue_selectors parses chain-qualified selectors", "[api]")
{
  const auto selectors = watpocket::parse_residue_selectors("A:12,B:18");
  REQUIRE(selectors.size() == 2);
  REQUIRE(selectors[0].chain.value() == "A");
  REQUIRE(selectors[0].resid == 12);
  REQUIRE(selectors[1].chain.value() == "B");
  REQUIRE(selectors[1].resid == 18);
}

TEST_CASE("parse_residue_selectors rejects empty selector", "[api]")
{
  REQUIRE_THROWS_AS(watpocket::parse_residue_selectors(","), watpocket::Error);
}

