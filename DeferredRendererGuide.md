# Deferred Renderer Guide

This is the deferred rendering plan, with each idea explained from the ground up. Part 1 covers the concepts, Part 2 the step-by-step instructions, Part 3 the mistakes beginners usually hit.

---

# Part 1: Concepts you need first

## 1.1 What your renderer does today: forward rendering

Right now each Sponza triangle goes through `simple_shader.frag`, and that shader loops over **every light** for **every fragment**. A *fragment* is a candidate pixel: one triangle covering one pixel.

The problem is **overdraw**. Sponza has pillars behind pillars behind walls. The GPU often shades a fragment, runs the full light loop, and then a closer triangle drawn later covers that pixel. All that lighting work is thrown away.

The cost is roughly `(fragments drawn) × (number of lights)`. With 6 lights this is fine. With 500 it is not.

## 1.2 The deferred idea: split *what is here* from *how it is lit*

Deferred rendering splits the work into two passes.

**Pass 1, the G-buffer pass.** Draw all geometry, but don't compute lighting. For each pixel, just **write down the facts** the lighting will need later:
- **Albedo:** the surface's base color, meaning the texture times the vertex color.
- **Normal:** which way the surface faces.
- **Position:** where in the world this pixel is.

These facts go into several screen-sized images. Together they are called the **G-buffer** ("geometry buffer"). The depth test still runs, so when the pass finishes, each pixel of the G-buffer holds only the **closest** surface. Everything hidden was overwritten.

**Pass 2, the lighting pass.** Draw **one triangle that covers the whole screen**. Its fragment shader runs exactly **once per pixel**: it reads the facts for that pixel from the G-buffer, runs the light loop and writes the final color.

The light loop now runs `(pixels on screen) × (lights)` times, no matter how much overdraw the scene has. This is the main reason deferred rendering exists.

**The catch: transparency.** A G-buffer pixel holds **one** surface, but a semi-transparent object needs two (itself and what's behind it). So transparent things are drawn **after** lighting, the old forward way, on top of the result. In your engine those are the point light billboards, because they use alpha blending.

## 1.3 Multiple render targets (MRT)

So far your fragment shader has one output, `layout(location = 0) out vec4 outColor`. A fragment shader can have several: `location = 0`, `location = 1`, `location = 2`, …, and each goes to a different image.

In Vulkan this means:
- `vkCmdBeginRendering` gets an **array** of `VkRenderingAttachmentInfo`, one per output. Array index 0 receives `location = 0`, and so on.
- The pipeline must know **in advance** how many color outputs there are and their formats, in `VkPipelineRenderingCreateInfo::pColorAttachmentFormats`. The pipeline and `vkCmdBeginRendering` must agree **exactly**, otherwise validation errors.
- The pipeline also needs one `VkPipelineColorBlendAttachmentState` per output. Blending is set per attachment. For the G-buffer it is off for all of them, because you want raw values written, not mixed.

## 1.4 Two ways a shader can use an image

Until now you have used images in two ways:
1. **As an attachment.** The pipeline writes into it at the end (color output, depth test). The swapchain image and the depth image work this way.
2. **As a sampled texture.** A shader reads it with `texture(sampler, uv)`. The Sponza textures work this way.

The G-buffer images are used **both ways**: attachment in pass 1, sampled texture in pass 2. Three consequences:
- The image must be **created** with both usage flags: `VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT`. Vulkan only lets you use an image the ways you declared at `vkCreateImage`.
- Between the passes the image must change **layout**, from `COLOR_ATTACHMENT_OPTIMAL` to `SHADER_READ_ONLY_OPTIMAL`. A layout is how the GPU arranges and compresses the pixels in memory, and the best arrangement for writing differs from the best one for reading.
- There must be a **barrier** so the lighting shader doesn't read before the G-buffer writes have finished.

**Your render graph already does both the layout change and the barrier.** When a pass lists an image as an *input*, `execute()` transitions it to `SHADER_READ_ONLY_OPTIMAL` with `src stage = COLOR_ATTACHMENT_OUTPUT, dst stage = FRAGMENT_SHADER`. That is exactly "wait for the G-buffer writes, then let the lighting fragment shader read". This is why building the graph first was worthwhile.

## 1.5 Load and store operations

Every attachment in `vkCmdBeginRendering` has two settings.

**`loadOp`, what happens to the old pixels when the pass starts:**
- `CLEAR`: fill with `clearValue`.
- `LOAD`: keep what was there, so you can draw on top.
- `DONT_CARE`: "I'll overwrite everything, the old contents don't matter." This is the fastest.

**`storeOp`, what happens to the pixels when the pass ends:**
- `STORE`: keep them in memory, because someone needs them later.
- `DONT_CARE`: "nobody needs these." The GPU may throw them away.

Today `ForwardPass` uses depth `storeOp = DONT_CARE`, which is correct now because nothing reads depth after it. In the deferred frame the billboard pass **does** need the depth that the G-buffer pass produced, so it can hide lights behind pillars. So:
- the G-buffer pass must **STORE** depth;
- the billboard pass must **LOAD** depth and **LOAD** the backbuffer (otherwise it would clear away your lit image).

This is a classic beginner bug. If it goes wrong you see black screens, or lights showing through walls.

## 1.6 Descriptors and samplers

A shader can't hold a `VkImageView` directly. It reads images through **descriptors**:
- A **descriptor set layout** (`vkCreateDescriptorSetLayout`) is the *shape*: "binding 0 is a combined image sampler, binding 1 is…". It must match the `layout(set = X, binding = Y)` lines in the shader.
- A **descriptor pool** (`vkCreateDescriptorPool`) is the memory the sets are allocated from. It is created with fixed capacities: max number of sets, and how many descriptors of each type. If you exceed them, allocation fails with `VK_ERROR_OUT_OF_POOL_MEMORY`.
- A **descriptor set** (`vkAllocateDescriptorSets`) is the actual table. `vkUpdateDescriptorSets` writes into it: "binding 0 = *this* image view, in *this* layout, with *this* sampler".
- `vkCmdBindDescriptorSets` plugs the table into the shader for the next draws.

A **sampler** (`vkCreateSampler`) is a small object that says *how* to read a texture:
- **filtering:** `LINEAR` blends neighboring pixels, `NEAREST` takes exactly one;
- **addressing:** what happens outside the 0–1 range (repeat, clamp, …).

Your texture sampler in `Image.cpp` uses `LINEAR` + `REPEAT`, which is right for the Sponza walls. For the G-buffer you want `NEAREST` + `CLAMP_TO_EDGE`. Blending the normal of a pillar edge with the wall behind it gives a normal that belongs to neither surface, which shows up as light halos along edges.

**Why descriptors are the hard part here:** a descriptor set stores the *handle* of an image view. When the window is resized, `resizeResources()` destroys the old G-buffer images and views and creates new ones. The descriptor set still holds the **old, destroyed** handles, and the lighting pass would read freed memory. It must be rewritten (`vkUpdateDescriptorSets`) every time the images are recreated.

Your `DescriptorWriter` already has `overwrite()` for exactly this.

## 1.7 The fullscreen triangle

The lighting pass needs a shape that covers every pixel. The obvious choice is a quad (two triangles). The usual trick is **one oversized triangle** instead. Its corners sit at screen positions (−1,−1), (3,−1) and (−1,3) in normalized device coordinates. The screen is the −1…1 square, so this triangle covers it completely, and the hardware clips away the parts outside.

Why not a quad: the GPU shades pixels in 2×2 blocks. Along the quad's diagonal, those blocks get shaded **twice**, once for each triangle. One triangle has no diagonal.

It needs no vertex buffer. The vertex shader computes the three corners from `gl_VertexIndex` (0, 1, 2), just as `point_light.vert` computes its billboard corners from `OFFSETS[gl_VertexIndex]`. It also outputs a UV coordinate going 0→1 across the screen, which the fragment shader uses to read the G-buffer.

## 1.8 sRGB, briefly

- Your textures are created as `R8G8B8A8_SRGB` (`Image.cpp:141`), and so is your swapchain (`B8G8R8A8_SRGB`).
- An `_SRGB` format converts automatically: reading converts *from* the sRGB curve to linear numbers, and writing converts *back*.
- Lighting math must happen on linear numbers, and these conversions keep it that way.

For the albedo G-buffer image, use `R8G8B8A8_SRGB` too. The shader writes linear color, the hardware stores it sRGB-encoded (which looks better in 8 bits, with less banding in dark areas), and the lighting shader reads it back as linear. Using `UNORM` here would show up as banding in Sponza's darker corners.

---

# Part 2: Step-by-step instructions

Each step ends with a **check**. Don't start the next step until the check passes. When something breaks, you'll then know which step caused it.

## Step 1 (warm-up): split ForwardPass into two passes

**Goal:** test LOAD ops and the graph's pass-to-pass ordering while the image stays the same.

1. Create a `SolidPass` that draws only `solid.renderGameObjects`. Outputs `{"backbuffer","depth"}`. Color CLEAR, depth CLEAR, **depth STORE**.
2. Change `ForwardPass` to draw only `lights.render`. Outputs `{"backbuffer","depth"}`. Color **LOAD**, depth **LOAD**.
3. In `Application::run()`, add `SolidPass` first, then `ForwardPass`.

**What the graph does now:**
- `compile()` sees that `ForwardPass` writes resources that `SolidPass` wrote, so it adds an edge Solid → Forward.
- In `execute()`, before `ForwardPass`, `transitionImage` sees the same layout but with an earlier *write*. So it records a barrier anyway, color output → color output and depth tests → depth tests. That barrier makes the solid pass's writes visible to the billboard pass.

**Check:** the image looks identical, and running with sync validation (`VK_KHRONOS_VALIDATION_VALIDATE_SYNC=true`) shows no errors. Then deliberately set `ForwardPass`'s color loadOp to CLEAR: only the lights should be left on black. Set it back. This shows you what LOAD does.

## Step 2: let `Pipeline` support several color outputs

**Where:** `PipelineConfigInfo` in `Pipeline.hpp`, and `defaultPipelineConfigInfo` in `Pipeline.cpp`.

**Now:** one `colorAttachmentFormat`, `colorAttachmentCount = 1`, one `colorBlendAttachment`, `colorBlendInfo.attachmentCount = 1`.

**Change:**
1. Replace the single color format with a list (vector) of formats, and the single blend attachment with a list of blend states.
2. `defaultPipelineConfigInfo` takes the list of color formats. For each format it adds one blend state (blend off, write mask RGBA).
3. `renderingInfo.colorAttachmentCount` and `colorBlendInfo.attachmentCount` become the list's size.
4. `enableAlphaBlending` must turn on blending for every entry in the list.

**Pitfall: dangling pointers.** `pColorAttachmentFormats` and `pAttachments` are raw pointers into those lists. A `std::vector` moves its memory when it grows. If you set the pointer and *then* add an element, the pointer points at freed memory, and pipeline creation crashes or silently gets garbage formats.

There is a second trap. `PipelineConfigInfo` is filled in one place and passed by reference elsewhere. If it's ever **copied**, the pointers still point into the *original*. The safest approach is to set those pointers inside `Pipeline::createGraphicsPipeline` right before `vkCreateGraphicsPipelines`. Your code already does this with `pNext = &configInfo.renderingInfo`, so it's the same idea.

**Check:** update the existing callers (`SolidRenderSystem`, `PointLightRenderSystem`) to pass a list with one format. Everything should look exactly as before.

## Step 3: let passes react when their images are (re)created

**Why:** the lighting pass's descriptor set must point at the current G-buffer views (section 1.6). The views exist only **after** `compile()` (which calls `allocateResources()`), and they are replaced on every `resizeResources()`. The pass needs to be told when either happens.

**Change:**
1. Add a virtual method to `RenderPass`, for example "resources allocated". It is empty by default, so `ForwardPass` and others don't need to implement it.
   - Its parameter is a lookup function like `RenderPassContext::get`. The pass asks by name and gets back the `RenderResource`, including its new `imageView`.
2. In `RenderGraph`, call that method on every pass:
   - at the end of `compile()`, after `allocateResources()`;
   - at the end of `resizeResources()`, after `allocateResources()`.

**Why rewriting the descriptor set inside `resizeResources()` is safe:** Vulkan forbids changing a descriptor set while a submitted command buffer that uses it is still running. `resizeResources()` runs from `recreateSwapChain()`, which calls `vkDeviceWaitIdle()` first, so the GPU is completely idle at that moment.

Write a comment there saying so. If someone later removes that `vkDeviceWaitIdle`, the comment explains what breaks.

**Check:** nothing to see yet. Put a temporary `std::cout` in an empty override and confirm it prints once at startup and once on every window resize.

## Step 4: the G-buffer resources

**Where:** `Application::run()`, next to where `depth` is registered.

Register three new resources through `renderer.addScreenSizedResource()`, so they follow the window size:

| Name | Format | Usage flags | Holds |
|---|---|---|---|
| `albedo` | `R8G8B8A8_SRGB` | color attachment + sampled | `texture × vertex color`, alpha = 1 |
| `normal` | `R16G16B16A16_SFLOAT` | color attachment + sampled | world-space normal in xyz; **alpha = 1 where geometry exists** |
| `position` | `R16G16B16A16_SFLOAT` | color attachment + sampled | world-space position in xyz |

Leave `aspectMask` at its default (COLOR). `depth` stays as it is.

**Why these formats:**
- **Normals:** have components between −1 and +1. A `UNORM` format can only store 0 to 1, so negative values would be clamped to 0. `SFLOAT` stores negative numbers directly.
- **Positions:** can be any value in the world (−5.3, 12.0, …), so they need a float format too. 16-bit floats are precise enough for a scene Sponza's size. If you see blocky or striped lighting far away, switch to `R32G32B32A32_SFLOAT`.
- **Format support:** all three formats are guaranteed by the Vulkan spec to work as color attachments and as sampled images, so you don't have to query support.

**Why alpha = 1 in `normal`:** you'll clear the G-buffer to alpha 0. In the lighting pass, a pixel whose normal alpha is still 0 means "no geometry was drawn here, this is sky or background". There you output the background color instead of running lighting on garbage.

**Check:** run it. `compile()` now allocates 4 images. There should be no validation errors, even though nothing uses the new images yet.

## Step 5: the G-buffer shaders

**`gbuffer.vert`:** a copy of `simple_shader.vert`. It already outputs everything needed: world position, world normal, vertex color, UV.

**`gbuffer.frag`:**
- **inputs:** same as `simple_shader.frag`;
- **bindings:** keep `set 0` (global UBO, even if unused here, so the pipeline layout stays the same) and `set 1` (the material texture);
- **outputs:** three, at `location = 0`, `1`, `2`, in the **same order** as the attachment array you'll pass to `vkCmdBeginRendering`:
  - location 0 → albedo: `texture(texSampler, texCoord).rgb × fragColor`, alpha 1;
  - location 1 → normal: the **normalized** `fragNormalWorldSpace`, alpha 1;
  - location 2 → position: `fragPosWorldSpace`, alpha 1;
- **no light loop at all.**

Add both shaders to however you compile `.spv` files now.

## Step 6: G-buffer render system and pass

**Render system:**
- The draw loop in `SolidRenderSystem` is exactly what the G-buffer needs: bind set 0, push the model matrix, bind each submesh's texture as set 1, `vkCmdDrawIndexed`. Only the **shaders** and the **output formats** differ.
- Instead of copying the class, make its constructor take the vertex and fragment shader paths and the list of color formats from step 2.
- Then create it in `Application::run()` with the `gbuffer` shaders and the formats `{albedo format, normal format, position format}`, plus the depth format.

**`GBufferPass`:**
- **Outputs:** `{"albedo","normal","position","depth"}`.
- **In `record()`:**
  - look up all four resources;
  - build 3 color attachment infos, all CLEAR + **STORE**: albedo cleared to the old background color (0.01, 0.01, 0.01), normal and position cleared to all zeros (alpha 0 marks "nothing here");
  - build the depth attachment info: CLEAR to 1.0 + **STORE**;
  - call `beginDynamicRendering` with color count 3;
  - draw the solids, then `vkCmdEndRendering`.
- **Order of the color array:** must match the shader's `location` numbers and the order of formats in the pipeline.

Replace `SolidPass` from step 1 with this pass.

**Check, without the lighting pass yet:** the screen shows only billboards on black. Expected, because nothing writes the backbuffer except the billboards.

To see the G-buffer, use **RenderDoc** (free; it works with Vulkan on Linux):
1. Launch the engine from RenderDoc and capture a frame.
2. In the Texture Viewer, open `albedo`, `normal` and `position`.
   - Albedo should look like an unlit Sponza.
   - Normal should be colorful: red, green and blue walls depending on direction.
   - Position should show smooth color gradients.

I recommend learning RenderDoc now. It is the most useful debugging tool in graphics programming.

## Step 7: the lighting system

Create a new `LightingSystem` class in `core/systems`. It owns everything the lighting pass needs to draw.

**1. Descriptor set layout for the G-buffer.** Three bindings (0, 1, 2), each a `COMBINED_IMAGE_SAMPLER` for the fragment stage.

**2. Pipeline layout.** Two sets: `{globalSetLayout, gbufferSetLayout}`.
- Set 0 is the same global UBO layout the other systems use, so the lighting shader can read `ubo.pointLights` and the camera position from `ubo.inverseViewMatrix`.
- No push constants yet. In step 9 you'll add one for debug views.

**3. Pipeline**, from `defaultPipelineConfigInfo`, then change:
- clear the binding and attribute descriptions (no vertex buffer, like the point light system);
- **depth test off and depth write off**: there's no depth attachment in this pass, and nothing to sort;
- cull mode none;
- color formats: `{swapchain format}`, because it writes the backbuffer;
- depth format `VK_FORMAT_UNDEFINED` (no depth attachment in this pass);
- shaders: `fullscreen.vert` + `lighting.frag`.

**4. Sampler.** `vkCreateSampler` with `NEAREST` min/mag filter, `CLAMP_TO_EDGE` on all axes, no anisotropy, no mipmaps. Destroy it in the destructor.

**5. Descriptor pool + one set.**
- **Own pool:** give it its own small pool (1 set, 3 combined image samplers) instead of making `globalPool` bigger. The system then owns its descriptors completely.
- **One set is enough** even with 2 frames in flight: the frames only *read* the G-buffer, and the set only changes when the GPU is idle (step 3).

**6. A method that writes the set** from three image views: each binding gets image view + `SHADER_READ_ONLY_OPTIMAL` + the sampler. The first call allocates the set (`build`), and later calls `overwrite` it.

**7. A render method:** bind the pipeline, bind set 0 (`frameInfo.globalDescriptorSet`) and set 1 (G-buffer), then `vkCmdDraw(commandBuffer, 3, 1, 0, 0)`. Three vertices, one triangle.

## Step 8: the lighting shaders

**`fullscreen.vert`:**
- no inputs;
- for `gl_VertexIndex` 0, 1, 2, output corners (−1,−1), (3,−1), (−1,3) with matching UVs (0,0), (2,0), (0,2);
- output the UV for the fragment shader.

Draw the triangle on paper and you'll see the 0–1 UV square lands exactly on the screen.

**`lighting.frag`:**
- **Bindings:** set 0, binding 0 is the global UBO, copied exactly from `simple_shader.frag`. Set 1, bindings 0/1/2 are `sampler2D` albedo, normal, position.
- **Read the G-buffer:** sample all three at the UV.
  - Alternative: `texelFetch(texture, ivec2(gl_FragCoord.xy), 0)` reads the exact pixel by integer coordinate, with no filtering at all. It's slightly more robust than UV sampling; either works with a NEAREST sampler.
- **Background:** if normal alpha is 0, output the albedo (the cleared background color) and stop.
- **Otherwise, move the light loop over from `simple_shader.frag` almost unchanged.** Where the old shader used `fragNormalWorldSpace`, `fragPosWorldSpace` and `texture(...) × fragColor`, use the three G-buffer values instead. Ambient, diffuse, specular and attenuation stay the same.

  Final color: `albedo × (diffuse + specular)`. The old shader multiplied both terms by `fragColor`, and that color is already part of the albedo now.

## Step 9: the lighting pass

**`LightingPass`**:
- holds `FrameInfo&` and `LightingSystem&`;
- **inputs:** `{"albedo","normal","position"}`. Listing them as inputs is what makes the graph do the layout change and barrier from section 1.4;
- **outputs:** `{"backbuffer"}`;
- **resource hook** (from step 3): look up the three resources and give their image views to the lighting system's write method;
- **`record()`:** one color attachment, the backbuffer. Use loadOp CLEAR or DONT_CARE (the triangle overwrites every pixel anyway) and STORE. No depth attachment, so pass a null depth pointer to `beginDynamicRendering`. Call the lighting system's render method, then `vkCmdEndRendering`.

**Order in `Application::run()`:** `GBufferPass`, `LightingPass`, `ForwardPass` (billboards, LOAD/LOAD from step 1), then `compile()`.

**Strongly recommended: a debug view.**
- Add a push constant (one int) to the lighting pipeline layout and shader.
- 0 means normal lighting; 1, 2 and 3 output plain albedo, plain normal and plain position.
- Bind number keys in the main loop to switch modes.
- Get modes 1–3 looking right *before* trusting mode 0. When the lighting looks wrong, you can then tell whether the G-buffer is wrong or the lighting math is.

**Check:**
- Mode 0 should look the same as your old forward renderer, side by side with a screenshot from before.
- Small differences in very dark areas can come from the sRGB albedo storage; that's expected.
- Billboards appear on top, hidden correctly behind pillars.

## Step 10: the billboard pass and depth

`ForwardPass` already outputs `{"backbuffer","depth"}` with LOAD/LOAD from step 1, so it needs no changes.

**The edges and barriers the graph builds:**
- `backbuffer`: the last writer is `LightingPass`, so there is an edge Lighting → Forward and a write-after-write barrier.
- `depth`: the last writer is `GBufferPass`, so there is an edge GBuffer → Forward and a write-after-write barrier on depth.

**Two optional improvements:**
- **Depth write off in the billboard pipeline.** The billboards should be *hidden by* geometry (depth test on) but shouldn't write depth, so they never hide each other.
- **Remove `simple_shader.*`** once you're happy, along with the `solid` reference in `ForwardPass`.

## Step 11: verification

1. **Sync validation on:** `VK_KHRONOS_VALIDATION_VALIDATE_SYNC=true ./VulkanEngine`. Expect no `SYNC-HAZARD` messages.
2. **Resize the window many times**, small and large.
   - This tests the step 3 hook. If the descriptor set isn't rewritten, you'll see validation errors about destroyed image views, or a corrupted image.
3. **Minimize and restore.** Your `recreateSwapChain()` waits while the window has size 0, so check that it comes back correctly.
4. **RenderDoc capture:** look at the event list and confirm the barriers between passes are the ones described above.
5. **Update `docs/VulkanMentalMap.tex`** with the new pass table, the resource hook, and who owns which sampler and pool.

---

# Part 3: Common beginner mistakes and what they look like

| Symptom | Likely cause |
|---|---|
| Validation error: attachment count/format mismatch | Pipeline's color format list ≠ attachments in `vkCmdBeginRendering` (count, order or format) |
| Validation error: image used as sampled but missing usage | Forgot `SAMPLED_BIT` on a G-buffer resource |
| `VK_ERROR_OUT_OF_POOL_MEMORY` | Pool too small for the new set or descriptors |
| Everything black after lighting | Lighting pass's descriptor set never written, or written before `compile()` allocated the images |
| Works until resize, then garbage or crash | Descriptor set not rewritten after `resizeResources()` |
| Lights shine through walls | Depth was `DONT_CARE` in the G-buffer pass, or `CLEAR` in the billboard pass |
| Only billboards on black | Billboard pass clears the backbuffer instead of LOAD |
| Lighting looks inverted or half the scene dark | Normals stored in a UNORM format (negatives clamped), or not normalized |
| Bright or dark halos along object edges | G-buffer sampled with a LINEAR sampler instead of NEAREST |
| Background shows weird lighting | No "nothing here" check on normal alpha |
| Banding in dark areas | Albedo stored as UNORM instead of SRGB |
| Random crash in pipeline creation | Dangling pointer to a resized vector in `PipelineConfigInfo` (step 2) |

---

## Where this leads

Once this works, the natural next steps are:
1. **Reconstruct position from depth** and delete the `position` image. This is a good exercise in matrix math.
2. **HDR:** lighting writes to a float image, and a final "tonemap" pass maps it to the screen. Needed before PBR, because realistic lights produce values far above 1.0.
3. **PBR:** the G-buffer gains metallic and roughness, and the lighting shader gets new math. The pass structure stays the same.
4. **Many lights:** move the lights from the UBO (`MAX_LIGHTS 10`) to a storage buffer.

Steps 2 and 3 (pipeline lists and the resource hook) are the only changes to engine infrastructure. Everything after that reuses patterns you've already built.
