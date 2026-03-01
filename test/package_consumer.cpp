#include <watpocket/watpocket.hpp>

int main()
{
  // Compile/link check: public header must not require chemfiles/cgal.
  (void)watpocket::build_version();
  return 0;
}

