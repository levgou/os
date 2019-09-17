#include "types.h"
#include "stat.h"
#include "user.h"

int
main(int argc, char *argv[])
{
  if(argc != 2) {
    printf(2, "not valid...\n");
    exit(1);
  }
  int pol = atoi(argv[1]);
  if(pol < 1 || pol > 3) {
    printf(2, "not valid...\n");
    exit(1);
  }

  policy(pol);
  exit(0);
}