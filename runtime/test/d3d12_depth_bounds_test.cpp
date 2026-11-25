// Depth bounds (D3D12_FEATURE_D3D12_OPTIONS2.DepthBoundsTestSupported): a PSO from a pipeline state stream whose
// DEPTH_STENCIL1 subobject enables the depth-bounds test, and OMSetDepthBounds values applied per draw with
// vkCmdSetDepthBounds. A range that excludes the stored depth leaves the render target untouched; one that includes
// it writes; a pipeline without the test ignores the range; and the range stays in effect across later draws.
//
// The shaders are GLSL written to the translator's contract (test/glsl/vs_depth.vert and fs_solid.frag). No game data.
#include "vk_test_device.h"
#include "d3d12_test_shaders.h"
#include "../shims/d3d12_com.h"
#include "../src/shader_library.h"

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#if !defined(LEONIDA_TEST_VS_DEPTH_SPV) || !defined(LEONIDA_TEST_FS_SOLID_SPV)
#error "LEONIDA_TEST_VS_DEPTH_SPV and LEONIDA_TEST_FS_SOLID_SPV must name the compiled test shaders"
#endif

#define EXPECT(condition) do {                                          \
    if (!(condition)) {                                                \
        std::fprintf(stderr, "line %d: %s\n", __LINE__, #condition);   \
        return 1;                                                      \
    }                                                                  \
} while (0)

using namespace d3d12test;

namespace {

constexpr UINT side = 32;
int g_failures = 0;

// One pipeline state stream subobject as d3dx12.h declares it (CD3DX12_PIPELINE_STATE_STREAM_SUBOBJECT).
template <D3D12_PIPELINE_STATE_SUBOBJECT_TYPE Type, typename Inner>
struct alignas(void*) Subobject {
    D3D12_PIPELINE_STATE_SUBOBJECT_TYPE type = Type;
    Inner inner;
};
using RootSubobject = Subobject<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_ROOT_SIGNATURE, ID3D12RootSignature*>;
using VsSubobject = Subobject<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_VS, D3D12_SHADER_BYTECODE>;
using PsSubobject = Subobject<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_PS, D3D12_SHADER_BYTECODE>;
using TopologySubobject = Subobject<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_PRIMITIVE_TOPOLOGY,
                                    D3D12_PRIMITIVE_TOPOLOGY_TYPE>;
using TargetsSubobject = Subobject<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_RENDER_TARGET_FORMATS, D3D12_RT_FORMAT_ARRAY>;
using DepthFormatSubobject = Subobject<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL_FORMAT, DXGI_FORMAT>;
using DepthStencil1Subobject = Subobject<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL1,
                                          D3D12_DEPTH_STENCIL_DESC1>;
using RasterizerSubobject = Subobject<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_RASTERIZER, D3D12_RASTERIZER_DESC>;

struct Stream {
    std::vector<std::uint8_t> bytes;
    template <typename S>
    Stream& add(const S& subobject) {
        static_assert(sizeof(S) % sizeof(void*) == 0);
        const std::size_t at = bytes.size();
        bytes.resize(at + sizeof(S));
        std::memcpy(bytes.data() + at, &subobject, sizeof(S));
        return *this;
    }
    D3D12_PIPELINE_STATE_STREAM_DESC desc() { return {bytes.size(), bytes.data()}; }
};

std::uint32_t f2u(float f) {
    std::uint32_t u;
    std::memcpy(&u, &f, 4);
    return u;
}

// What one render records: an optional first range, then an optional second range and a second draw, so a list can
// show that OMSetDepthBounds stays in effect for later draws.
struct Bounds {
    bool set = false;
    float min = 0.0f;
    float max = 1.0f;
};

struct Harness {
    ID3D12Device* device = nullptr;
    ID3D12RootSignature* root = nullptr;
    std::vector<std::uint8_t> vs, ps;
    Ref<ID3D12Resource> target, depth, readback;
    Ref<ID3D12DescriptorHeap> rtv_heap, dsv_heap;
    D3D12_CPU_DESCRIPTOR_HANDLE rtv{}, dsv{};
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
    Ref<ID3D12CommandQueue> queue;
    Ref<ID3D12CommandAllocator> allocator;
    Ref<ID3D12GraphicsCommandList> list;
    Ref<ID3D12GraphicsCommandList1> list1;
    Ref<ID3D12Fence> fence;
    UINT64 fence_value = 0;
    std::uint8_t* readback_ptr = nullptr;
    D3D12_RESOURCE_STATES target_state = D3D12_RESOURCE_STATE_COMMON;
    bool depth_in_write = false;
    const FLOAT clear_color[4] = {0.0f, 0.0f, 1.0f, 1.0f};
    const std::uint8_t clear_bytes[4] = {0, 0, 255, 255};

    Ref<ID3D12PipelineState> make_pso(bool depth_bounds) {
        Ref<ID3D12Device2> device2;
        Ref<ID3D12PipelineState> pso;
        if (FAILED(device->QueryInterface(IID_PPV_ARGS(&device2.p)))) return pso;

        D3D12_DEPTH_STENCIL_DESC1 ds{};
        ds.DepthEnable = TRUE;
        ds.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
        ds.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
        ds.StencilEnable = FALSE;
        ds.DepthBoundsTestEnable = depth_bounds ? TRUE : FALSE;
        D3D12_RASTERIZER_DESC raster{};
        raster.FillMode = D3D12_FILL_MODE_SOLID;
        raster.CullMode = D3D12_CULL_MODE_NONE;
        raster.DepthClipEnable = TRUE;
        D3D12_RT_FORMAT_ARRAY formats{};
        formats.NumRenderTargets = 1;
        formats.RTFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;

        Stream stream;
        stream.add(RootSubobject{.inner = root})
            .add(VsSubobject{.inner = {vs.data(), vs.size()}})
            .add(PsSubobject{.inner = {ps.data(), ps.size()}})
            .add(TopologySubobject{.inner = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE})
            .add(TargetsSubobject{.inner = formats})
            .add(DepthFormatSubobject{.inner = DXGI_FORMAT_D32_FLOAT})
            .add(RasterizerSubobject{.inner = raster})
            .add(DepthStencil1Subobject{.inner = ds});
        const D3D12_PIPELINE_STATE_STREAM_DESC desc = stream.desc();
        device2->CreatePipelineState(&desc, IID_PPV_ARGS(&pso.p));
        return pso;
    }

    // Draws the full-screen triangle at depth z once per Bounds that is set, then copies the target to `out` (RGBA
    // bytes) and waits for the GPU: a second Bounds makes a second draw follow the second OMSetDepthBounds. The depth
    // buffer is cleared to `clear_depth`: the depth-bounds test compares that stored value, not the fragment's own
    // depth.
    bool run(ID3D12PipelineState* pso, Bounds first, Bounds second, float z, float clear_depth, std::uint8_t out[4],
             std::string& error) {
        if (FAILED(allocator->Reset()) || FAILED(list->Reset(allocator.p, pso))) {
            error = "list reset failed";
            return false;
        }
        if (target_state != D3D12_RESOURCE_STATE_RENDER_TARGET) {
            const D3D12_RESOURCE_BARRIER to_target = transition(target.p, target_state,
                                                                D3D12_RESOURCE_STATE_RENDER_TARGET);
            list->ResourceBarrier(1, &to_target);
            target_state = D3D12_RESOURCE_STATE_RENDER_TARGET;
        }
        if (!depth_in_write) {
            const D3D12_RESOURCE_BARRIER to_depth = transition(depth.p, D3D12_RESOURCE_STATE_COMMON,
                                                               D3D12_RESOURCE_STATE_DEPTH_WRITE);
            list->ResourceBarrier(1, &to_depth);
            depth_in_write = true;
        }
        list->ClearRenderTargetView(rtv, clear_color, 0, nullptr);
        list->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, clear_depth, 0, 0, nullptr);
        list->SetGraphicsRootSignature(root);
        list->SetGraphicsRoot32BitConstant(0, f2u(z), 0);
        const D3D12_VIEWPORT viewport{0, 0, float(side), float(side), 0, 1};
        const D3D12_RECT scissor{0, 0, LONG(side), LONG(side)};
        list->RSSetViewports(1, &viewport);
        list->RSSetScissorRects(1, &scissor);
        list->OMSetRenderTargets(1, &rtv, FALSE, &dsv);
        list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        if (first.set) list1->OMSetDepthBounds(first.min, first.max);
        list->DrawInstanced(3, 1, 0, 0);
        if (second.set) {
            list1->OMSetDepthBounds(second.min, second.max);
            list->DrawInstanced(3, 1, 0, 0);
        }
        const D3D12_RESOURCE_BARRIER to_copy = transition(target.p, target_state, D3D12_RESOURCE_STATE_COPY_SOURCE);
        list->ResourceBarrier(1, &to_copy);
        target_state = D3D12_RESOURCE_STATE_COPY_SOURCE;

        D3D12_TEXTURE_COPY_LOCATION dst{};
        dst.pResource = readback.p;
        dst.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        dst.PlacedFootprint = footprint;
        D3D12_TEXTURE_COPY_LOCATION src{};
        src.pResource = target.p;
        src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        src.SubresourceIndex = 0;
        list->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
        if (list->Close() != S_OK) {
            error = "Close failed";
            return false;
        }
        ID3D12CommandList* lists[] = {list.p};
        queue->ExecuteCommandLists(1, lists);
        if (rt::d3d12::last_execute_result(queue.p) != leonida::ReplayResult::success) {
            error = "the layer refused the list";
            return false;
        }
        if (FAILED(queue->Signal(fence.p, ++fence_value)) || FAILED(fence->SetEventOnCompletion(fence_value, nullptr))) {
            error = "fence wait failed";
            return false;
        }
        std::uint8_t* mapped = readback_ptr;
        if (!mapped && FAILED(readback->Map(0, nullptr, reinterpret_cast<void**>(&mapped)))) {
            error = "Map failed";
            return false;
        }
        readback_ptr = mapped;
        std::memcpy(out, mapped + footprint.Offset + (side / 2) * footprint.Footprint.RowPitch + (side / 2) * 4, 4);
        return true;
    }
};

void expect_pixel(const Harness& h, const std::uint8_t got[4], bool drawn) {
    const std::uint8_t r = drawn ? 32 : h.clear_bytes[0];
    const std::uint8_t g = drawn ? 160 : h.clear_bytes[1];
    const std::uint8_t b = drawn ? 240 : h.clear_bytes[2];
    if (got[0] == r && got[1] == g && got[2] == b && got[3] == 255) {
        std::printf("PASS %s\n", drawn ? "draw wrote" : "bounds rejected the draw");
    } else {
        std::fprintf(stderr, "FAIL %s: got %u,%u,%u,%u want %u,%u,%u,255\n",
                     drawn ? "draw wrote" : "bounds rejected the draw", got[0], got[1], got[2], got[3], r, g, b);
        ++g_failures;
    }
}

}  // namespace

int main() {
    vktest::Device d;
    d.request_depth_bounds = true;
    const auto device_status = d.initialize();
    if (device_status == vktest::DeviceStatus::unavailable) {
        std::fprintf(stderr, "SKIP: no usable Vulkan 1.3 device\n");
        return 77;
    }
    EXPECT(device_status == vktest::DeviceStatus::ready);
    if (!d.depth_bounds) {
        std::fprintf(stderr, "SKIP: the GPU has no core depthBounds feature\n");
        return 77;
    }

    const auto vs_program = fssm_program({0x56530db0, 0xbf810000});
    const auto ps_program = fssm_program({0x50530db0, 0xbf810000});
    const auto vs = shader_container(vs_program), ps = shader_container(ps_program);
    const auto library = std::filesystem::temp_directory_path() / "leonida_depth_bounds_test";
    std::filesystem::create_directories(library);
    const auto copy = std::filesystem::copy_options::overwrite_existing;
    EXPECT(std::filesystem::copy_file(LEONIDA_TEST_VS_DEPTH_SPV, library / "vs_depth.spv", copy));
    EXPECT(std::filesystem::copy_file(LEONIDA_TEST_FS_SOLID_SPV, library / "fs_solid.spv", copy));
    {
        std::ofstream index(library / "index.json", std::ios::binary);
        index << "{\"vs_depth\": {\"stage\": \"vs\", \"fssm_sha256\": \"" << leonida::sha256_hex(vs_program)
              << "\", \"bindings\": [], \"push_constant_bytes\": 144, \"input_signature\": []},\n"
                 " \"fs_solid\": {\"stage\": \"ps\", \"fssm_sha256\": \"" << leonida::sha256_hex(ps_program)
              << "\", \"bindings\": [], \"push_constant_bytes\": 144, \"outputs\": [0]}}";
    }

    rt::d3d12::VulkanDeviceInfo info{};
    info.get_instance_proc = d.instance_proc;
    info.instance = d.instance;
    info.physical = d.physical;
    info.device = d.device;
    info.queue = d.queue;
    info.queue_family = d.family;
    info.buffer_device_address = true;
    info.shader_dir = library.string();
    info.depth_bounds = d.depth_bounds;
    Ref<ID3D12Device> device;
    EXPECT(SUCCEEDED(rt::d3d12::create_vulkan_device(info, IID_PPV_ARGS(&device.p))));

    D3D12_FEATURE_DATA_D3D12_OPTIONS2 options{};
    EXPECT(SUCCEEDED(device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS2, &options, sizeof(options))));
    if (options.DepthBoundsTestSupported != TRUE) {
        std::fprintf(stderr, "FAIL OPTIONS2 DepthBoundsTestSupported is not TRUE on a depthBounds device\n");
        return 1;
    }
    std::printf("PASS OPTIONS2 DepthBoundsTestSupported\n");

    // A root signature with one 32-bit constant: vs_depth reads s0 as the clip-space depth.
    D3D12_ROOT_PARAMETER parameter{};
    parameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    parameter.Constants = {0, 0, 1};
    parameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    D3D12_VERSIONED_ROOT_SIGNATURE_DESC root_desc{};
    root_desc.Version = D3D_ROOT_SIGNATURE_VERSION_1_0;
    root_desc.Desc_1_0 = {1, &parameter, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_NONE};
    const auto root_blob = serialize_root_signature(root_desc);
    EXPECT(!root_blob.empty());
    Ref<ID3D12RootSignature> root;
    EXPECT(SUCCEEDED(device->CreateRootSignature(0, root_blob.data(), root_blob.size(), IID_PPV_ARGS(&root.p))));

    Harness h;
    h.device = device.p;
    h.root = root.p;
    h.vs = vs;
    h.ps = ps;

    const D3D12_HEAP_PROPERTIES default_heap{D3D12_HEAP_TYPE_DEFAULT};
    const D3D12_HEAP_PROPERTIES readback_heap{D3D12_HEAP_TYPE_READBACK};
    D3D12_RESOURCE_DESC target_desc{};
    target_desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    target_desc.Width = target_desc.Height = side;
    target_desc.DepthOrArraySize = 1;
    target_desc.MipLevels = 1;
    target_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    target_desc.SampleDesc.Count = 1;
    target_desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    EXPECT(SUCCEEDED(device->CreateCommittedResource(&default_heap, D3D12_HEAP_FLAG_NONE, &target_desc,
                                                     D3D12_RESOURCE_STATE_COMMON, nullptr,
                                                     IID_PPV_ARGS(&h.target.p))));
    D3D12_RESOURCE_DESC depth_desc = target_desc;
    depth_desc.Format = DXGI_FORMAT_D32_FLOAT;
    depth_desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
    EXPECT(SUCCEEDED(device->CreateCommittedResource(&default_heap, D3D12_HEAP_FLAG_NONE, &depth_desc,
                                                     D3D12_RESOURCE_STATE_COMMON, nullptr,
                                                     IID_PPV_ARGS(&h.depth.p))));
    const D3D12_RESOURCE_DESC readback_desc = buffer_desc(side * 256);
    EXPECT(SUCCEEDED(device->CreateCommittedResource(&readback_heap, D3D12_HEAP_FLAG_NONE, &readback_desc,
                                                     D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
                                                     IID_PPV_ARGS(&h.readback.p))));
    UINT rows = 0;
    UINT64 row_size = 0, total = 0;
    device->GetCopyableFootprints(&target_desc, 0, 1, 0, &h.footprint, &rows, &row_size, &total);
    EXPECT(rows == side && total <= side * 256 && h.footprint.Footprint.Width == side &&
           h.footprint.Footprint.RowPitch >= side * 4);

    const D3D12_DESCRIPTOR_HEAP_DESC rtv_heap_desc{D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 1, D3D12_DESCRIPTOR_HEAP_FLAG_NONE, 0};
    const D3D12_DESCRIPTOR_HEAP_DESC dsv_heap_desc{D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1, D3D12_DESCRIPTOR_HEAP_FLAG_NONE, 0};
    EXPECT(SUCCEEDED(device->CreateDescriptorHeap(&rtv_heap_desc, IID_PPV_ARGS(&h.rtv_heap.p))));
    EXPECT(SUCCEEDED(device->CreateDescriptorHeap(&dsv_heap_desc, IID_PPV_ARGS(&h.dsv_heap.p))));
    h.rtv = h.rtv_heap->GetCPUDescriptorHandleForHeapStart();
    h.dsv = h.dsv_heap->GetCPUDescriptorHandleForHeapStart();
    device->CreateRenderTargetView(h.target.p, nullptr, h.rtv);
    device->CreateDepthStencilView(h.depth.p, nullptr, h.dsv);

    D3D12_COMMAND_QUEUE_DESC queue_desc{};
    queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    EXPECT(SUCCEEDED(device->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&h.queue.p))));
    EXPECT(SUCCEEDED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&h.allocator.p))));
    EXPECT(SUCCEEDED(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, h.allocator.p, nullptr,
                                               IID_PPV_ARGS(&h.list.p))));
    EXPECT(SUCCEEDED(h.list->QueryInterface(IID_PPV_ARGS(&h.list1.p))));
    EXPECT(SUCCEEDED(h.list->Close()));  // CreateCommandList starts recording; the first run resets it
    EXPECT(SUCCEEDED(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&h.fence.p))));

    Ref<ID3D12PipelineState> bounds_pso = h.make_pso(true);
    Ref<ID3D12PipelineState> plain_pso = h.make_pso(false);
    if (!bounds_pso.p || !plain_pso.p) {
        std::fprintf(stderr, "FAIL a depth-bounds pipeline state could not be created\n");
        return 1;
    }
    std::printf("PASS pipeline state stream with DepthBoundsTestEnable\n");

    std::string error;
    std::uint8_t pixel[4] = {};
    const auto run = [&](ID3D12PipelineState* pso, Bounds first, Bounds second, float z, float clear_depth) {
        error.clear();
        const bool ok = h.run(pso, first, second, z, clear_depth, pixel, error);
        if (!ok) std::fprintf(stderr, "run failed: %s\n", error.c_str());
        return ok;
    };
    // The depth-bounds test compares the depth value already in the buffer (D3D12 and Vulkan agree), so each case
    // clears the depth to the value the range is meant to include or exclude.
    // 1: the stored 0.5 is outside 0..0.1, so the draw writes nothing.
    EXPECT(run(bounds_pso.p, {true, 0.0f, 0.1f}, {}, 0.5f, 0.5f));
    expect_pixel(h, pixel, false);
    // 2: 0.5 is inside 0.4..0.6, so the draw writes.
    EXPECT(run(bounds_pso.p, {true, 0.4f, 0.6f}, {}, 0.5f, 0.5f));
    expect_pixel(h, pixel, true);
    // 3: a stored 0.9 is outside 0.4..0.6, so the draw writes nothing even though the fragment's own depth is 0.5.
    EXPECT(run(bounds_pso.p, {true, 0.4f, 0.6f}, {}, 0.5f, 0.9f));
    expect_pixel(h, pixel, false);
    // 4: a pipeline without the test ignores the range.
    EXPECT(run(plain_pso.p, {true, 0.4f, 0.6f}, {}, 0.5f, 0.9f));
    expect_pixel(h, pixel, true);
    // 5: no OMSetDepthBounds leaves D3D12's initial 0..1 range, which contains the stored 0.5.
    EXPECT(run(bounds_pso.p, {}, {}, 0.5f, 0.5f));
    expect_pixel(h, pixel, true);
    // 6: an inverted range (min > max) makes every sample fail, as D3D12 documents.
    EXPECT(run(bounds_pso.p, {true, 0.6f, 0.4f}, {}, 0.5f, 0.5f));
    expect_pixel(h, pixel, false);
    // 7: a range outside 0..1 clamps to the whole range (the stored value is always in 0..1).
    EXPECT(run(bounds_pso.p, {true, -1.0f, 2.0f}, {}, 0.5f, 0.5f));
    expect_pixel(h, pixel, true);
    // 8: the range set before the first draw is replaced before the second; only the second passes, so the final
    //    colour shows that the new range was in effect for the later draw.
    EXPECT(run(bounds_pso.p, {true, 0.0f, 0.1f}, {true, 0.4f, 0.6f}, 0.5f, 0.5f));
    expect_pixel(h, pixel, true);

    if (g_failures) {
        std::fprintf(stderr, "FAIL %d depth-bounds checks\n", g_failures);
        return 1;
    }
    std::printf("PASS depth bounds on the GPU\n");
    return 0;
}
