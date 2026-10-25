// macos_main.mm - DBEngine macOS 原生窗口后端(Cocoa)：软件渲染内核 + NSWindow 显示。
#import <Cocoa/Cocoa.h>
#include "../engine/framebuffer.hpp"
#include "../engine/voxel.hpp"
#include "../engine/texture.hpp"
#include <cmath>
using namespace dbg;

static Framebuffer g_fb(640, 360);

static void renderFrame(float yaw) {
    static VoxelRenderer vr;
    static TexturePack tp;
    static bool init = false;
    if (!init) init = true;   // 默认贴图
    World w(48, 32, 48); w.generate();
    Camera3 cam; cam.pos = V3(24, 26, 24); cam.yaw = yaw; cam.pitch = -0.15f;
    vr.render(g_fb, cam, w, 14, tp);
}

@interface DBView : NSView
@end
@implementation DBView
- (void)drawRect:(NSRect)dirtyRect {
    // 引擎帧缓冲是 RGBA 内存序，直接做成 CGImage 画进窗口
    CGDataProviderRef prov = CGDataProviderCreateWithData(NULL, g_fb.px.data(),
        g_fb.px.size() * sizeof(uint32_t), NULL);
    CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
    CGImageRef img = CGImageCreate(640, 360, 8, 32, 640 * 4, cs,
        kCGBitmapByteOrderDefault | kCGImageAlphaLast, prov, NULL, false,
        kCGRenderingIntentDefault);
    CGContextRef ctx = [[NSGraphicsContext currentContext] CGContext];
    CGContextDrawImage(ctx, CGRectMake(0, 0, 640, 360), img);
    CGImageRelease(img); CGColorSpaceRelease(cs); CGDataProviderRelease(prov);
}
@end

int main() {
    NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];
    [NSApplication sharedApplication];
    NSWindow* win = [[NSWindow alloc] initWithContentRect:NSMakeRect(0,0,640,360)
        styleMask:(NSWindowStyleMaskTitled|NSWindowStyleMaskClosable|NSWindowStyleMaskMiniaturizable)
        backing:NSBackingStoreBuffered defer:NO];
    [win setTitle:@"DBEngine - macOS"];
    DBView* view = [[DBView alloc] initWithFrame:NSMakeRect(0,0,640,360)];
    [win setContentView:view];
    [win center]; [win makeKeyAndOrderFront:nil];
    [win setReleasedWhenClosed:NO];
    [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
    [NSApp activateIgnoringOtherApps:YES];

    float yaw = 0.6f;
    while (1) {
        renderFrame(yaw); yaw += 0.008f;
        [view setNeedsDisplay:YES];
        NSEvent* ev = [NSApp nextEventMatchingMask:NSEventMaskAny
                    untilDate:[NSDate dateWithTimeIntervalSinceNow:0.016]
                    inMode:NSDefaultRunLoopMode dequeue:YES];
        if (ev) [NSApp sendEvent:ev];
    }
    [pool drain];
    return 0;
}
