// macOS host glue: CAMetalLayer on the NSWindow content view.
// The core (libsumi) sets the layer's device/pixelFormat/drawableSize — the
// host only creates and attaches it (§5.1 ownership contract).
#include "metal_layer_glue.h"

#import <Cocoa/Cocoa.h>
#import <QuartzCore/CAMetalLayer.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_COCOA
#include <GLFW/glfw3native.h>

extern "C" {

void* sumi_macos_attach_metal_layer(GLFWwindow* window) {
    NSWindow* nswindow = glfwGetCocoaWindow(window);
    if (!nswindow) return NULL;
    CAMetalLayer* layer = [CAMetalLayer layer];
    if (!layer) return NULL;
    NSView* view = nswindow.contentView;
    view.wantsLayer = YES;
    view.layer = layer;
    layer.contentsScale = nswindow.backingScaleFactor;
    return (__bridge_retained void*)layer;
}

void sumi_macos_detach_metal_layer(GLFWwindow* window, void* layer_ptr) {
    if (!layer_ptr) return;
    CAMetalLayer* layer = (__bridge_transfer CAMetalLayer*)layer_ptr;
    NSWindow* nswindow = window ? glfwGetCocoaWindow(window) : nil;
    if (nswindow && nswindow.contentView.layer == layer) {
        nswindow.contentView.layer = nil;
        nswindow.contentView.wantsLayer = NO;
    }
    // ARC releases `layer` here.
}

} // extern "C"
