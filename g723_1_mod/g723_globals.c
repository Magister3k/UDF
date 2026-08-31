/*
 * Global variable definitions for G.723.1 codec
 * Extracted from LBCCODE2.C to avoid main() conflict
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