/* Author: Anshunkang Zhou <azhouad@cse.ust.hk>
 * File Description: 
 * Creation Date: March 04, 2024
 * Modification History:
 */

 #include "Support/Options.h"

 /// plankton-dasm
 
 const Option<bool> Options::Split(
         "split",
         "Whether to use split module for parallelism.",
         {TOOL::DASM},
         false
 );
 
 