#include <stdint.h>

extern "C" {
    void _ZN7android14SurfaceControl8setLayerEj(uint32_t);
    void _ZN7android13GraphicBufferC1Ejjij(uint32_t inWidth, uint32_t inHeight, int32_t inFormat, uint32_t inUsage);	

    void _ZN7android14SurfaceControl8setLayerEi(int32_t layer){
        _ZN7android14SurfaceControl8setLayerEj(static_cast<uint32_t>(layer));
    }
}