// Software decoding of BC1–BC7 textures, for GPUs that cannot sample them (MTLDevice.supportsBCTextureCompression is NO: Apple A16 and older iPhone / iPad GPUs).
// nvn_fmt_to_mtl maps each BC format to the uncompressed format below while nvn_bc_soft is set, and the uploads decode the blocks into it (nvn_mtl.m).
// Pure C, no dependency on Metal headers; values are MTLPixelFormat numbers.
#pragma once
#include <stddef.h>
#include <stdint.h>

extern int nvn_bc_soft;   // 1 = BC textures are decoded on the CPU (set once the Metal device is known; DM_BC_SOFT=1 forces it, =0 turns it off)

// The uncompressed Metal format a BC Metal format decodes to and its bytes per pixel; 0 when mtl_pf is no BC format
//   BC1 / BC2 / BC3 / BC7 (and sRGB) → ASTC 4x4 LDR (sRGB), or RGBA8Unorm (_sRGB) with nvn_bc_astc 0; BC4 → R8Unorm / R8Snorm, BC5 → RG8Unorm / RG8Snorm,
//   BC6H → RG11B10Float (unsigned, nvn_bc_astc) / RGBA16Float
uint32_t bc_soft_target(uint32_t mtl_pf, int *bytes_per_pixel);
// Rows of the decoded level in the staging layout: ceil(h / 4) * 4 pixel rows, or ceil(h / 4) block rows for ASTC 4x4 (nvn_bc_astc)
size_t bc_soft_rows(uint32_t mtl_pf, uint32_t h);
// Target-aware variant: 3D textures use uncompressed storage because ASTC 3D support is not portable.
uint32_t bc_soft_target_for_texture(uint32_t mtl_pf, int is_3d, int *bytes_per_pixel);
size_t bc_soft_rows_for_texture(uint32_t mtl_pf, uint32_t h, int is_3d);
extern int nvn_bc_astc;   // 1 (the default) = BC1 / 2 / 3 / 7 become ASTC 4x4 and BC6H unsigned RG11B10F; 0 = RGBA8 / RGBA16F

// Decode a w × h region of BC blocks (src: rows of 4 × 4 blocks, src_pitch bytes from one block row to the next) into dst (dst_pitch bytes per pixel row).
// dst must hold whole blocks: ceil(w / 4) * 4 pixels per row and ceil(h / 4) * 4 rows. Returns 0, or -1 when mtl_pf is no BC format
int bc_soft_decode(uint32_t mtl_pf, const uint8_t *src, size_t src_pitch, uint32_t w, uint32_t h, uint8_t *dst, size_t dst_pitch);
int bc_soft_decode_for_texture(uint32_t mtl_pf, int is_3d, const uint8_t *src, size_t src_pitch, uint32_t w, uint32_t h, uint8_t *dst, size_t dst_pitch);

// One 4 × 4 block into 16 pixels (row-major, bytes_per_pixel each), for tests
int bc_soft_block(uint32_t mtl_pf, const uint8_t *block, uint8_t *out, size_t out_pitch);
// One 4 × 4 block as the upload writes it (an ASTC 4x4 block, RG11B10F pixels, or bc_soft_block's pixels), for tests
int bc_soft_out_block(uint32_t mtl_pf, const uint8_t *block, uint8_t *out, size_t out_pitch);
