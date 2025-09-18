#include <stdlib.h>
#include <stdio.h>

#ifdef CYARG_PICO_TARGET
#include <pico/stdlib.h>

#include "ws2812.pio.h"
#endif

#include "common.h"
#include "object.h"
#include "value.h"
#include "builtin.h"
#include "vm.h"


#define WS2812_GPIO 18

#ifdef CYARG_PICO_TARGET
struct ws2812pio {
    PIO pio;
    uint sm;
    uint offset;
};
#endif

bool ws2812initNative(ObjRoutine* routineContext, int argCount, Value* result) {
    bool success = false;
#ifdef CYARG_PICO_TARGET
    ObjBlob* handle = newBlob(sizeof(struct ws2812pio));
    struct ws2812pio* pio = (struct ws2812pio*) handle->blob;

    tempRootPush(OBJ_VAL(handle));

    success = pio_claim_free_sm_and_add_program(&ws2812_program, &pio->pio, &pio->sm, &pio->offset);
    if (success) {

        ws2812_program_init(pio->pio, pio->sm, pio->offset, WS2812_GPIO, 800000);
    }

    tempRootPop();

    if (success) {
        *result = OBJ_VAL(handle);
    }
#endif

    return success;
}

bool ws2812writepixelNative(ObjRoutine* routineContext, int argCount, Value* result) {
#ifdef CYARG_PICO_TARGET
    Value blobVal = nativeArgument(routineContext, argCount, 0);
    Value pixelVal = nativeArgument(routineContext, argCount, 1);

    if (!IS_BLOB(blobVal)) {
        runtimeError(routineContext, "Expected a WS2812 handle.");
        return false;
    }
    if (!IS_UI32(pixelVal)) {
        runtimeError(routineContext, "Expected an unsigned integer.");
        return false;
    }

    ObjBlob* blob = AS_BLOB(blobVal);
    struct ws2812pio* pio = (struct ws2812pio*) blob->blob;

    uint32_t pixel = AS_UI32(pixelVal);

    pio_sm_put_blocking(pio->pio, pio->sm, pixel);
#endif
    return true;
}