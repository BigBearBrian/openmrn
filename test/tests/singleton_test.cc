#include <stdio.h>
#include "gtest/gtest.h"
#include "os/os.h"

#include "utils/singleton.hxx"


class MyClass : public Singleton<MyClass> {
public:
  int x_;
};

DEFINE_SINGLETON_INSTANCE(MyClass);

TEST(SingletonTest, CreateAndGetSingle) {
  MyClass obj;
  EXPECT_EQ(&obj, MyClass::instance());
  EXPECT_EQ(&obj, MyClass::instance());
}

TEST(SingletonTest, CreateAndGetMultiple) {
  MyClass* p;
  {
    MyClass obj;
    p = &obj;
    EXPECT_EQ(p, MyClass::instance());
  }
  {
    MyClass obj2;
    EXPECT_EQ(&obj2, MyClass::instance());
    EXPECT_NE(p, &obj2);
  }
}

TEST(SingletonTest, DieEmptyRef) {
  MyClass* p;
  EXPECT_DEATH({
      p = MyClass::instance();
    }, "instance_ != nullptr");
  {
    MyClass obj;
    p = &obj;
    EXPECT_EQ(p, MyClass::instance());
  }
  EXPECT_DEATH({
      p = MyClass::instance();
    }, "instance_ != nullptr");
}

TEST(SingletonTest, DieMultipleInstances) {
  MyClass* p;
  MyClass obj;
  p = &obj;
  EXPECT_DEATH({
      MyClass obj2;
      p = MyClass::instance();
    }, "instance_ == nullptr");
}

int appl_main(int argc, char* argv[]) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
