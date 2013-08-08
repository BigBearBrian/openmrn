/** \copyright
 * Copyright (c) 2013, Balazs Racz
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  - Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 *  - Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * \file volatile_state_test.cc
 * Tests for volatile_state.hxx
 *
 * @author Balazs Racz
 * @date 21 July 2013
 */


#include <stdio.h>
#include "gtest/gtest.h"
#include "os/os.h"

#include "cue/volatile_state.hxx"

/*class VolatileStateTest : public testing::Test {

protected:
  VolatileState


  }*/


TEST(VolatileStateTest, SimpleSetAndGet) {
  VolatileState st(3);

  ASSERT_EQ(3U, st.size());

  st.Set(0, 0x2f);
  st.Set(1, 0x55);
  st.Set(2, 0xaa);

  EXPECT_EQ(0xaa, st.Get(2));
  EXPECT_EQ(0x55, st.Get(1));
  EXPECT_EQ(0x2f, st.Get(0));
}

TEST(VolatileStateTest, InitZeroed) {
  VolatileState st(3);

  EXPECT_EQ(0, st.Get(0));
  EXPECT_EQ(0, st.Get(1));
  EXPECT_EQ(0, st.Get(2));
}

TEST(VolatileStateTest, SetViaRef) {
  VolatileState st(3);

  st.GetRef(0) = 0x3f;
  st.GetRef(1) = 0x65;
  st.GetRef(2) = 0xba;

  EXPECT_EQ(0xba, st.Get(2));
  EXPECT_EQ(0x65, st.Get(1));
  EXPECT_EQ(0x3f, st.Get(0));
}

TEST(VolatileStateTest, GetViaRef) {
  VolatileState st(3);

  st.GetRef(2) = 0xba;

  uint8_t v = st.GetRef(2);
  EXPECT_EQ(0xba, v);
  EXPECT_EQ(0xba, st.GetRef(2));
}

TEST(VolatileStateTest, GetViaPtr) {
  VolatileState st(3);

  st.GetRef(2) = 0xba;

  EXPECT_EQ(0xba, *st.GetPtr(2));
}

TEST(VolatileStateTest, SetViaPtrIndex) {
  VolatileState st(3);
  EXPECT_EQ(0, *st.GetPtr(2));
  st.GetPtr(0)[2] = 0xb8;
  EXPECT_EQ(0xb8, *st.GetPtr(2));
}

TEST(VolatileStateTest, OutIndexDies) {
  VolatileState st(3);
  EXPECT_DEATH(st.Set(3, 0), "offset < size");
  EXPECT_DEATH(*st.GetPtr(3), "size");
  EXPECT_DEATH(st.GetRef(3), "size");
  // Having a pointer out of bounds is not an error in itself.
  VolatileStatePtr p = st.GetPtr(3);
  EXPECT_DEATH(*p = 4, "size");
  p = st.GetPtr(2);
  *p = 3;
  EXPECT_DEATH(p[1] = 4, "size");
}

int appl_main(int argc, char* argv[]) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}