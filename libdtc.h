#ifndef LIBDTC_H
#define LIBDTC_H

#include "dtc.h"


/* DeviceTreeInfo creation */

/**
 *  Take a new device tree, validate its values,
 *  fill in gaps where possible, and set defaults.
 *
 *  @param dti  the device tree to construct
*/
void dt_new(dt_info_t *dti);

/**
 *  Recursively copy everything from one device tree
 *  into a new one.
 * 
 *  Typically used for destructive operations where you
 *  need the original structure and/or data.
 * 
 *  @param dti  The DeviceTree to copy
 *  @return     The new copy. Must be freed by the caller
*/
dt_info_t *dt_copy(dt_info_t *dti);


/* DeviceTree modifications */

/**
 *  Deallocate all memory used for a device tree.
 * 
 *  @param dti  The DeviceTree to free
*/
void dt_free(dt_info_t *dti);

#endif /* LIBDTC_H */
