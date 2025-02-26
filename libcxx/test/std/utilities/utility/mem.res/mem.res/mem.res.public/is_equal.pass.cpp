//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

// UNSUPPORTED: c++03, c++11, c++14

// <memory_resource>

//------------------------------------------------------------------------------
// TESTING virtual bool is_equal(memory_resource const &) const noexcept
//
// Concerns:
//   A) 'memory_resource' provides a function 'is_equal' with the required
//      signature.
//   B) 'is_equal' is noexcept.
//   C) 'do_is_equal' is called using the same arguments passed to 'is_equal'
//      and the resulting value is returned.
//   D) 'do_is_equal' is called on the LHS object and not the RHS object.

#include <memory_resource>
#include <cassert>
#include <type_traits>

#include "test_macros.h"
#include "test_std_memory_resource.h"

int main(int, char**) {
  {
    const std::pmr::memory_resource* r1 = nullptr;
    const std::pmr::memory_resource* r2 = nullptr;
    ASSERT_NOEXCEPT(r1->is_equal(*r2));
  }
  {
    TestResource1 R1(1);
    auto& P1                            = R1.getController();
    const std::pmr::memory_resource& M1 = R1;

    TestResource2 R2(1);
    auto& P2                            = R2.getController();
    const std::pmr::memory_resource& M2 = R2;

    assert(M1.is_equal(M2) == false);
    assert(P1.checkIsEqualCalledEq(1));
    assert(P2.checkIsEqualCalledEq(0));

    assert(M2.is_equal(M1) == false);
    assert(P2.checkIsEqualCalledEq(1));
    assert(P1.checkIsEqualCalledEq(1));
  }
  {
    TestResource1 R1(1);
    auto& P1                            = R1.getController();
    const std::pmr::memory_resource& M1 = R1;

    TestResource1 R2(2);
    auto& P2                            = R2.getController();
    const std::pmr::memory_resource& M2 = R2;

    assert(M1.is_equal(M2) == false);
    assert(P1.checkIsEqualCalledEq(1));
    assert(P2.checkIsEqualCalledEq(0));

    assert(M2.is_equal(M1) == false);
    assert(P2.checkIsEqualCalledEq(1));
    assert(P1.checkIsEqualCalledEq(1));
  }
  {
    TestResource1 R1(1);
    auto& P1                            = R1.getController();
    const std::pmr::memory_resource& M1 = R1;

    TestResource1 R2(1);
    auto& P2                            = R2.getController();
    const std::pmr::memory_resource& M2 = R2;

    assert(M1.is_equal(M2) == true);
    assert(P1.checkIsEqualCalledEq(1));
    assert(P2.checkIsEqualCalledEq(0));

    assert(M2.is_equal(M1) == true);
    assert(P2.checkIsEqualCalledEq(1));
    assert(P1.checkIsEqualCalledEq(1));
  }

  return 0;
}
