#include <cstdio>

#include <openssl/sha.h>

int main()
{
  const unsigned char str[] = "Original String";
  unsigned char hash[SHA_DIGEST_LENGTH]; // == 20

  SHA1(str, sizeof(str) - 1, hash);

  for (int i = 0; i < SHA_DIGEST_LENGTH; ++i)
    printf("%02x", hash[i]);
  printf("\n");

  return 0;
}
