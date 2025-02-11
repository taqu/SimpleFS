#include "catch_amalgamated.hpp"
#include "../simplefs.h"

#define EQ_FLOAT(x0, x1) CHECK(std::abs(x0-x1)<1.0e-7f)

TEST_CASE("Builder" "[build]")
{
	SECTION("step"){
		sfs::Builder builder;
		builder.open("out.pak");
		builder.build("../");
    }
}

