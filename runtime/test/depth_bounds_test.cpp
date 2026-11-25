// Depth bounds in the command-list IR and its Vulkan replay: a draw records the list's OMSetDepthBounds min/max, and
// replay emits vkCmdSetDepthBounds only when that draw's pipeline enables the depth-bounds test. Every Vulkan entry
// point is a no-op, except the one under test, which records its arguments: no GPU, driver or game files are involved.
#ifndef VK_NO_PROTOTYPES
#define VK_NO_PROTOTYPES
#endif
#include "../src/vulkan_replay.h"

#include <cstdint>
#include <cstdio>
#include <type_traits>

#define CHECK(condition) do {                                          \
    if (!(condition)) {                                                \
        std::fprintf(stderr, "line %d: %s\n", __LINE__, #condition);   \
        return 1;                                                      \
    }                                                                  \
} while (0)

using namespace leonida;

namespace {

constexpr std::uintptr_t bounds_pipeline_id = 1;
constexpr std::uintptr_t plain_pipeline_id = 2;

int g_bounds_calls = 0;
float g_bounds_min[4] = {};
float g_bounds_max[4] = {};

void VKAPI_CALL record_depth_bounds(VkCommandBuffer, float min, float max) {
    if (g_bounds_calls < 4) {
        g_bounds_min[g_bounds_calls] = min;
        g_bounds_max[g_bounds_calls] = max;
    }
    ++g_bounds_calls;
}

template<typename F>
struct Nop;
template<typename R, typename... A>
struct Nop<R(A...)> {
    static R VKAPI_CALL call(A...) {
        if constexpr (!std::is_void_v<R>) return R{};
    }
};
template<typename PFN>
PFN nop() {
    return &Nop<std::remove_pointer_t<PFN>>::call;
}

GraphicsPipeline g_bounds_pipeline;
GraphicsPipeline g_plain_pipeline;

const GraphicsPipeline* graphics_pipeline(std::uintptr_t id) {
    if (id == bounds_pipeline_id) return &g_bounds_pipeline;
    if (id == plain_pipeline_id) return &g_plain_pipeline;
    return nullptr;
}

Draw draw(std::uintptr_t pipeline, float min, float max) {
    Draw draw{};
    draw.pipeline = pipeline;
    draw.topology = 5;  // D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST
    draw.viewport = {0, 0, 64, 64, 0, 1};
    draw.scissor = {0, 0, 64, 64};
    draw.count = 3;
    draw.instances = 1;
    draw.depth_bounds_min = min;
    draw.depth_bounds_max = max;
    return draw;
}

ReplayFunctions replay_functions() {
    ReplayFunctions vk{};
    vk.graphics_pipeline = graphics_pipeline;
    vk.image_view = [](const ResourceBinding&, const VkImageSubresourceRange&) { return VkImageView{}; };
    vk.begin_rendering = nop<PFN_vkCmdBeginRendering>();
    vk.end_rendering = nop<PFN_vkCmdEndRendering>();
    vk.bind_pipeline = nop<PFN_vkCmdBindPipeline>();
    vk.push_constants = nop<PFN_vkCmdPushConstants>();
    vk.bind_vertex_buffers2 = nop<PFN_vkCmdBindVertexBuffers2>();
    vk.bind_index_buffer = nop<PFN_vkCmdBindIndexBuffer>();
    vk.set_viewport = nop<PFN_vkCmdSetViewport>();
    vk.set_scissor = nop<PFN_vkCmdSetScissor>();
    vk.set_primitive_topology = nop<PFN_vkCmdSetPrimitiveTopology>();
    vk.set_primitive_restart_enable = nop<PFN_vkCmdSetPrimitiveRestartEnable>();
    vk.set_stencil_reference = nop<PFN_vkCmdSetStencilReference>();
    vk.set_blend_constants = nop<PFN_vkCmdSetBlendConstants>();
    vk.set_depth_bounds = record_depth_bounds;
    vk.draw = nop<PFN_vkCmdDraw>();
    vk.draw_indexed = nop<PFN_vkCmdDrawIndexed>();
    vk.buffer_at = [](std::uint64_t, std::uint64_t&, VkBuffer&, VkDeviceSize&) { return false; };
    return vk;
}

ReplayResult replay_list(const CommandList& list, const ReplayFunctions& vk) {
    return replay(list, reinterpret_cast<VkCommandBuffer>(std::uintptr_t(1)),
                  [](ResourceId) -> const ResourceBinding* { return nullptr; }, vk);
}

}  // namespace

int main() {
    // A draw starts with D3D12's full 0..1 range, and a recorded draw keeps the values it captured.
    const Draw defaults{};
    CHECK(defaults.depth_bounds_min == 0.0f && defaults.depth_bounds_max == 1.0f);
    Draw captured = draw(bounds_pipeline_id, 0.25f, 0.75f);
    CommandList record;
    CHECK(record.draw(std::move(captured)));
    const auto& recorded = std::get<Draw>(record.commands().front());
    CHECK(recorded.depth_bounds_min == 0.25f && recorded.depth_bounds_max == 0.75f);

    g_bounds_pipeline.pipeline = reinterpret_cast<VkPipeline>(std::uintptr_t(0x100));
    g_bounds_pipeline.depth_bounds_test = true;
    g_plain_pipeline.pipeline = reinterpret_cast<VkPipeline>(std::uintptr_t(0x200));

    // Two draws through a bounds-testing pipeline: the in-range one applies its values; the draw through a pipeline
    // without the test emits nothing; the inverted range (which D3D12 accepts and makes every sample fail) sets no
    // Vulkan bounds, since Vulkan has no valid inverted range and this draw has no depth attachment to discard on.
    // The depth_bounds_empty pipeline variant that covers a bound depth buffer is exercised by d3d12_depth_bounds_test.
    CommandList list;
    CHECK(list.draw(draw(bounds_pipeline_id, 0.25f, 0.75f)));
    CHECK(list.draw(draw(plain_pipeline_id, 0.1f, 0.2f)));
    CHECK(list.draw(draw(bounds_pipeline_id, 0.8f, 0.2f)));
    CHECK(list.close());
    ReplayFunctions vk = replay_functions();
    CHECK(replay_list(list, vk) == ReplayResult::success);
    CHECK(g_bounds_calls == 1);
    CHECK(g_bounds_min[0] == 0.25f && g_bounds_max[0] == 0.75f);

    // Without vkCmdSetDepthBounds a bounds-testing pipeline cannot be replayed; a plain one still can.
    vk.set_depth_bounds = nullptr;
    g_bounds_calls = 0;
    CHECK(replay_list(list, vk) == ReplayResult::unsupported);
    CHECK(g_bounds_calls == 0);
    CommandList plain;
    CHECK(plain.draw(draw(plain_pipeline_id, 0.1f, 0.2f)));
    CHECK(plain.close());
    CHECK(replay_list(plain, vk) == ReplayResult::success);

    std::printf("PASS depth-bounds record and replay\n");
    return 0;
}
