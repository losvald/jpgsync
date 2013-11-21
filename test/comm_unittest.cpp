#include <unordered_set>

#include "../src/comm.hpp"

#include "test.hpp"

using namespace std;

TEST(ExifHashConstructorTest, Sha1Digest) {
  unsigned char sha1_digest[20 + 1] =
      "\xca\xfe\xba\xbe"
      "\x42\x23\xf3\x00"
      "\xa5\x00\x00\xff"
      "\x51\xd0\xca\x73"
      "\x02\xeb\x84\x69";
  ExifHash ef(sha1_digest);
  std::ostringstream oss; oss << ef;
  ASSERT_EQ("cafebabe4223f300a50000ff51d0ca7302eb8469", oss.str());
}

TEST(ExifHashCollisionTest, SameHash) {
  ExifHash ef1((const unsigned char*) "abcd" "efgh" "ijkl" "mnop" "qrst");
  ExifHash ef2((const unsigned char*) "abcd" "efgh" "ijkl" "mnop" "qrst");
  std::unordered_set<ExifHash> s;
  s.insert(ef1);
  s.insert(ef2);
  ASSERT_EQ(1, s.size());
  EXPECT_EQ(std::hash<ExifHash>()(ef1), std::hash<ExifHash>()(ef2));
  EXPECT_EQ(ef1, ef2);

  ExifHash ef3((const unsigned char*) "abcd" "efgH" "ijkl" "mnop" "qrst");
  s.insert(ef3);
  ASSERT_EQ(2, s.size());
  EXPECT_NE(ef1, ef3);
  EXPECT_NE(std::hash<ExifHash>()(ef2), std::hash<ExifHash>()(ef3));
}
