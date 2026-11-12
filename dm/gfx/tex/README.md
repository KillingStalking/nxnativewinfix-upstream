# dm/gfx/tex — texture format mapping, block-linear deswizzling, BC decompression

The part of the graphics layer that deals with texture data, called by the replay layer `../nvn/nvn_mtl.m`. It does three things:

1. converts NVN texture format numbers to Metal pixel formats;
2. unpacks the Switch's "block-linear" layout into an ordinary row-by-row layout;
3. decompresses BC textures on GPUs that do not support BC compressed textures.

Block linear is how the Tegra X1 GPU stores textures: the data is not one row after another, but interleaved in small blocks of 64 bytes × 8 rows (GOBs), and several GOBs are then stacked into larger blocks according to a "block height" and a "block depth". The game's packaged texture files are in exactly this layout, and it must be undone before uploading to Metal.

## Files

| File | Contents |
|---|---|
| `nvn_fmt_mtl.h`, `nvn_fmt_mtl.c` | `nvn_fmt_to_mtl()`: NVN format (0x01–0x9C) → Metal pixel format + component remapping. Returns 1 for an exact match, 2 for a substitute (precision, bit order or interpretation differs and a conversion is needed), 0 when Metal has no corresponding format. How each format is handled is written in the comments inside the table. Pure C, no dependency on Metal headers |
| `nvn_deswizzle.h`, `nvn_deswizzle.c` | `nvn_deswizzle()`: deswizzles one level and one layer from block linear to linear; `nvn_swizzle()`: the reverse, used for round trips in unit tests; `nvn_bl_offset()`: the block-linear address of a single point. Pure C |
| `nvn_deswizzle_gpu.h`, `nvn_deswizzle_gpu.m` | A Metal compute shader version of the same deswizzling algorithm, used by the replay layer when uploading packaged textures; when initialization fails or an unsupported format is encountered, the replay layer falls back to the CPU version |
| `tests/` | Unit tests |

The offsets of each level and layer within the storage, and the block height and block depth actually used by each level, are all taken from `../nvn/nvn_tex.c` (`nvn_tex_layout`, `nvn_tex_level_offset`, `nvn_tex_level_tiles`); this directory only does addressing and moving within a level. Packaged parameters override the automatic block dimensions: [Packaged texture block dimensions and linear storage rows](../../../docs/reference/nvn-objects.md#packaged-texture-block-dimensions-and-linear-storage-rows).

## BC textures

On a device without native BC texture support (`MTLDevice.supportsBCTextureCompression == NO`, including A16 and older iPhone / iPad GPUs), `nvn_bc_soft` switches the format mapping and upload path to software decompression. BC1 / BC2 / BC3 / BC7 are stored as ASTC 4×4 by default; BC4 / BC5 become R8 / RG8; unsigned BC6H becomes RG11B10F and signed BC6H becomes RGBA16F. BC1–BC3 can use the Metal compute transcoder when GPU deswizzling and its pipeline are available; otherwise the integer CPU transcoder is used. BC7 and the remaining formats use the software decoder. Sparse textures and 3D BC1 / BC2 / BC3 / BC7 use uncompressed pixel storage; ASTC is not assumed to be valid for 3D textures. `DM_BC_SOFT=1` forces the no-BC path for testing on a supported GPU, and `DM_BC_ASTC=0` selects uncompressed formats instead of ASTC / packed HDR output. A BC texture is never a render target and its raw blocks are not sampled through an uncompressed view.

## Tests

```sh
make -C dm/gfx/tex/tests run       # run t_fmt_mtl and t_tex
```

- **`t_fmt_mtl`**: takes the mapping for every NVN format in 0x01–0x9C and, for every one whose mapping is not 0, actually creates a texture on the local Metal device; all of them must be created successfully. It also checks that every NVN format with sRGB in its name lands on a Metal sRGB format. No game files needed.
- **`t_tex`**: the deswizzling tests, in several groups:

  | Group | Criterion |
  |---|---|
  | A round trip | Writes random data into each level and layer and reads it back out; must be byte-for-byte equal. Covers 2D, 2D array and 3D, several bytes-per-block values and non-power-of-two sizes |
  | B address formula | The result of `nvn_bl_offset()` equals the address computed by hand from the public layout rules |
  | D differential copies | Both copy directions match a byte-address oracle for 1,029 deterministic layouts, including padded pitches, byte tails, sparse mip tails, packaged block dimensions, arrays and cubes; untouched storage and row padding are compared too |
  | E hand-made 3D | Places markers at hand-computed addresses, checks where they land after deswizzling, and checks block height, block depth and storage size |

  The A round trip can only prove "one-to-one and within bounds"; it cannot prove that the layout formula is right. What backs it up is the hand-computed addresses of groups B and E, and the games that render right.

## Limitations

- `nvn_deswizzle()` does not support multisampled textures, textures with the LINEAR flag (which are linear to begin with) or the BUFFER target; it returns -1 when it encounters them.
- Cubes and cube arrays are handled as layered textures.
- The component bit order of the 16-bit packed formats (565 / 5551 / 4444) is mapped according to the format names and has not been checked with real data; they are marked as "substitute" in the format table.


## BC1–BC3 → ASTC 4×4 transcoding

On GPUs without BC support, BC1, BC2 and BC3 keep their compressed-size storage by being transcoded to ASTC 4×4 at upload. The CPU fallback uses the integer reference in `bc2astc_core.h` / `bc2astc_body.inc`; the same body is compiled as the Metal kernel in `bc2astc_msl.h`. CPU and GPU output must be byte-identical. BC7 continues to use the existing software ASTC encoder; BC4 / BC5 and signed BC6H retain their software-decoded formats.

The standalone Metal parity test is `dm/gfx/tests/t_bc2astc_gpu.m`. It exercises BC1–BC3 with pitched buffers and checks the GPU output against the C reference, including the transcoder's layout and endpoint-swap cases.
