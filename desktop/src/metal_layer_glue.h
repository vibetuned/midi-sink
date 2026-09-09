// Host-side macOS surface prep: create a CAMetalLayer and attach it to the
// GLFW window's content view (PROJECT_SPEC.md §1). The returned pointer is
// handed to sumi_create as native_surface_handle; the host must not touch the
// layer again until after sumi_destroy, when it detaches/releases it here.
#pragma once

typedef struct GLFWwindow GLFWwindow;

#ifdef __cplusplus
extern "C" {
#endif

// Returns a retained CAMetalLayer* (as void*), or NULL on failure.
void* sumi_macos_attach_metal_layer(GLFWwindow* window);

// Detach from the view and release the layer. Call after sumi_destroy.
void  sumi_macos_detach_metal_layer(GLFWwindow* window, void* layer);

// #58: SwiftUI's .hiddenTitleBar look for the canvas — the content view runs
// edge to edge under a transparent, title-less title bar; the traffic lights
// stay, edge resizing stays, and the window is still draggable by the (now
// invisible) title-bar strip. false restores the standard title bar.
void  sumi_macos_set_titlebar_hidden(GLFWwindow* window, int hidden);


#ifdef __cplusplus
}
#endif
