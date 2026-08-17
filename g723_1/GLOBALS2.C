/*
**
** File:        "globals2.c"
**
** Description:  Global variables initialization for G.723.1 codec
**
*/

#define __unix__

#include <stdio.h>
#include "typedef2.h"
#include "cst2.h"
#include "lbccode2.h"

/* Global encoder/decoder state variables that were missing from the codec */

/* From LBCCODE2.H */
Flag  UseHp  = 0;      /* Use highpass filter flag */
Flag  UsePf  = 1;      /* Use postfilter flag */
Flag  UseVx  = 1;      /* Use voice activity detector flag */

/* Encoder state for encoding operations */
CODSTATDEF CodStat = {0};
enum Crate WrkRate = Rate63;     /* Work rate enum - default Rate63 */

/*
** Function: Update_Acf (stub implementation)
** Updates autocorrelation coefficients for LPC analysis
*/
void Update_Acf(double *Acf_sf)
{
    /* This is a stub implementation for compatibility.
       The original codec may use this for adaptive LPC updates.
       Currently, we do nothing as the decoder doesn't need it. */
    (void)Acf_sf;  /* Avoid unused parameter warning */
}
