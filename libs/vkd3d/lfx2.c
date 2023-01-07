#define VKD3D_DBG_CHANNEL VKD3D_DBG_CHANNEL_API

#include "vkd3d_private.h"

static pthread_once_t library_once = PTHREAD_ONCE_INIT;
static struct vkd3d_lfx2_vtable lfx2_vtable;
static BOOL lfx2_available;

static void vkd3d_lfx2_load(void)
{
    HMODULE module = LoadLibraryA("latencyflex2_rust.dll");
    if (!module)
    {
        lfx2_available = false;
        return;
    }

#define LOAD_FUNCTION(name) lfx2_vtable.name = (void *)GetProcAddress(module, "lfx2" #name)

    LOAD_FUNCTION(Dx12ContextCreate);
    LOAD_FUNCTION(Dx12ContextRelease);
    LOAD_FUNCTION(Dx12ContextBeforeSubmit);
    LOAD_FUNCTION(Dx12ContextBeginFrame);
    LOAD_FUNCTION(Dx12ContextEndFrame);
    LOAD_FUNCTION(TimestampNow);
    LOAD_FUNCTION(TimestampFromQpc);
    LOAD_FUNCTION(ImplicitContextCreate);
    LOAD_FUNCTION(ImplicitContextRelease);
    LOAD_FUNCTION(ImplicitContextReset);
    LOAD_FUNCTION(FrameCreateImplicit);
    LOAD_FUNCTION(FrameDequeueImplicit);
    LOAD_FUNCTION(FrameRelease);

#undef LOAD_FUNCTION

    lfx2_available = true;
}

struct vkd3d_lfx2_vtable *vkd3d_lfx2_get_vtable(void)
{
    pthread_once(&library_once, vkd3d_lfx2_load);
    return lfx2_available ? &lfx2_vtable : NULL;
}

void vkd3d_lfx2_context_init(struct vkd3d_lfx2_context *context, d3d12_device_iface *device)
{
    struct vkd3d_lfx2_vtable *lfx2 = vkd3d_lfx2_get_vtable();
    if (!lfx2)
        return;

    pthread_mutex_init(&context->current_implicit_frame_lock, NULL);
    context->current_implicit_frame = NULL;
    context->dx12_context = lfx2->Dx12ContextCreate((ID3D12Device *)device);
    context->implicit_context = lfx2->ImplicitContextCreate();
}

void vkd3d_lfx2_context_free(struct vkd3d_lfx2_context *context)
{
    struct vkd3d_lfx2_vtable *lfx2 = vkd3d_lfx2_get_vtable();
    if (!lfx2)
        return;

    if (context->current_implicit_frame)
        lfx2->FrameRelease(context->current_implicit_frame);
    if (context->implicit_context)
        lfx2->ImplicitContextRelease(context->implicit_context);
    if (context->dx12_context)
        lfx2->Dx12ContextRelease(context->dx12_context);
    pthread_mutex_destroy(&context->current_implicit_frame_lock);
}