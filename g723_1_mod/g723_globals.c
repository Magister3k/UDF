/*
 * Global variable definitions for G.723.1 codec
 * Extracted from LBCCODE2.C to avoid main() conflict
 * Added stubs for encoder symbols referenced by decoder code
 */

#include <stdio.h>
#include "typedef2.h"
#include "cst2.h"
#include "lbccode2.h"

enum  Wmode   WrkMode = Both;
enum  Crate   WrkRate = Rate63;

Flag  UseHp = True;
Flag  UsePf = True;
Flag  UseVx = True;
Flag  UsePr = True;
int   ReinitSize = 0;

/* Encoder state structure referenced by decoder code (EXC2.c, UTIL2.c, LPC2.c) */
CODSTATDEF CodStat = {0};

/* Stub for Update_Acf - referenced by LPC2.c:Comp_Lpc */
void Update_Acf(FLOAT *Acf_sf)
{
    (void)Acf_sf;  /* Avoid unused parameter warning */
}