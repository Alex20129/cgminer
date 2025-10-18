#ifndef SCRYPT_H
#define SCRYPT_H

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "../cgminer.h"

void scrypt_regen_hash(struct work *work);

#endif /* SCRYPT_H */
