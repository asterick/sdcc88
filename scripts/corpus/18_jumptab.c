/* jump tables (genJumpTab): the BA table-offset addition + jp hl dispatch,
   and the byte-temp compare loop (gencjneshort PAIRPTR fallback).
   These were corpus blind spots: the z80 originals used DE/BC. */
int dense (int x)
{
  switch (x) {
  case 0: return 10; case 1: return 21; case 2: return 33; case 3: return 47;
  case 4: return 52; case 5: return 66; case 6: return 71; case 7: return 89;
  }
  return -1;
}
char dense_c (char x)
{
  switch (x) {
  case 0: return 'a'; case 1: return 'b'; case 2: return 'c';
  case 3: return 'd'; case 4: return 'e'; case 5: return 'f';
  }
  return '?';
}
int keep_across (int x, int keep)
{
  int r = 0;
  switch (x) { case 0: r = 1; break; case 1: r = 2; break; case 2: r = 3; break;
               case 3: r = 4; break; case 4: r = 5; break; case 5: r = 6; break; }
  return keep + r;
}
