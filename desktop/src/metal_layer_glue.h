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

// Set the Dock tile from an in-memory PNG (desktop/src/app_icon_macos.h).
// The harness is a bare executable — no .app bundle, no .icns — so the Dock
// icon can only be set at runtime; the macOS analog of glfwSetWindowIcon.
void  sumi_macos_set_dock_icon(const void* png_bytes, unsigned long png_len);

#ifdef __cplusplus
}
#endif
