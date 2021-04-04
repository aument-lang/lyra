#include <string.h>

#include "context.h"

void lyra_ctx_init(struct lyra_ctx *ctx) {
    memset(ctx, 0, sizeof(struct lyra_ctx));
}