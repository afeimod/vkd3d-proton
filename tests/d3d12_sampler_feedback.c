/*
 * Copyright 2016-2017 Józef Kucia for CodeWeavers
 * Copyright 2020-2023 Philip Rebohle for Valve Corporation
 * Copyright 2020-2023 Joshua Ashton for Valve Corporation
 * Copyright 2020-2023 Hans-Kristian Arntzen for Valve Corporation
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#define VKD3D_DBG_CHANNEL VKD3D_DBG_CHANNEL_API
#include "d3d12_crosstest.h"

void test_sampler_feedback_resource_creation(void)
{
    D3D12_FEATURE_DATA_D3D12_OPTIONS7 features7;
    D3D12_HEAP_PROPERTIES heap_props;
    D3D12_HEAP_DESC heap_desc;
    D3D12_RESOURCE_DESC1 desc;
    ID3D12Resource *resource;
    ID3D12Device8 *device8;
    ID3D12Device *device;
    ID3D12Heap *heap;
    unsigned int i;
    UINT ref_count;
    HRESULT hr;

    static const struct test
    {
        unsigned int width, height, layers, levels;
        D3D12_MIP_REGION mip_region;
        D3D12_RESOURCE_FLAGS flags;
        HRESULT expected;
    } tests[] = {
        { 9, 11, 5, 3, { 4, 4, 1 }, D3D12_RESOURCE_FLAG_NONE, S_OK /* This is invalid, but runtime does not throw an error here. We'll just imply UAV flag for these formats. */ },
        { 9, 11, 5, 3, { 4, 4, 1 }, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, S_OK },
        { 13, 11, 5, 3, { 5, 5, 1 }, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, E_INVALIDARG /* POT error */ },
        { 9, 11, 5, 3, { 0, 0, 0 }, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, E_INVALIDARG /* Must be at least 4x4 */ },
        { 9, 11, 5, 3, { 4, 4, 2 }, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, E_INVALIDARG /* Depth field must be 0 or 1. */ },
        { 9, 11, 5, 3, { 4, 4, 0 }, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, S_OK /* This is fine. */ },
        { 9, 11, 5, 3, { 8, 8, 0 }, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, E_INVALIDARG /* Must not be more than half the size. */ },
    };

    device = create_device();
    if (!device)
        return;

    if (FAILED(ID3D12Device_CheckFeatureSupport(device, D3D12_FEATURE_D3D12_OPTIONS7, &features7, sizeof(features7))) ||
            features7.SamplerFeedbackTier < D3D12_SAMPLER_FEEDBACK_TIER_0_9)
    {
        skip("Sampler feedback not supported.\n");
        ID3D12Device_Release(device);
        return;
    }

    hr = ID3D12Device_QueryInterface(device, &IID_ID3D12Device8, (void **)&device8);
    ok(SUCCEEDED(hr), "Failed to query Device8, hr #%x.\n", hr);

    memset(&desc, 0, sizeof(desc));
    memset(&heap_props, 0, sizeof(heap_props));
    memset(&heap_desc, 0, sizeof(heap_desc));

    heap_props.Type = D3D12_HEAP_TYPE_DEFAULT;
    desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.SampleDesc.Count = 1;
    desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;

    heap_desc.SizeInBytes = 1024 * 1024;
    heap_desc.Properties = heap_props;
    heap_desc.Flags = D3D12_HEAP_FLAG_CREATE_NOT_ZEROED | D3D12_HEAP_FLAG_ALLOW_ONLY_NON_RT_DS_TEXTURES;
    hr = ID3D12Device_CreateHeap(device, &heap_desc, &IID_ID3D12Heap, (void **)&heap);
    ok(SUCCEEDED(hr), "Failed to create heap, hr #%x.\n");

    for (i = 0; i < ARRAY_SIZE(tests); i++)
    {
        vkd3d_test_set_context("Test %u", i);
        desc.Width = tests[i].width;
        desc.Height = tests[i].height;
        desc.DepthOrArraySize = tests[i].layers;
        desc.MipLevels = tests[i].levels;
        desc.SamplerFeedbackMipRegion = tests[i].mip_region;
        desc.Flags = tests[i].flags;

        desc.Format = DXGI_FORMAT_SAMPLER_FEEDBACK_MIN_MIP_OPAQUE;

        hr = ID3D12Device8_CreateCommittedResource2(device8, &heap_props, D3D12_HEAP_FLAG_CREATE_NOT_ZEROED,
            &desc, D3D12_RESOURCE_STATE_COMMON, NULL, NULL, &IID_ID3D12Resource, (void **)&resource);
        ok(hr == tests[i].expected, "Unexpected hr, expected #%x, got #%x.\n", tests[i].expected, hr);
        if (SUCCEEDED(hr))
            ID3D12Resource_Release(resource);

        hr = ID3D12Device8_CreatePlacedResource1(device8, heap, 0, &desc, D3D12_RESOURCE_STATE_COMMON, NULL, &IID_ID3D12Resource, (void **)&resource);
        ok(hr == tests[i].expected, "Unexpected hr, expected #%x, got #%x.\n", tests[i].expected, hr);
        if (SUCCEEDED(hr))
            ID3D12Resource_Release(resource);

        desc.Format = DXGI_FORMAT_SAMPLER_FEEDBACK_MIP_REGION_USED_OPAQUE;

        hr = ID3D12Device8_CreateCommittedResource2(device8, &heap_props, D3D12_HEAP_FLAG_CREATE_NOT_ZEROED,
            &desc, D3D12_RESOURCE_STATE_COMMON, NULL, NULL, &IID_ID3D12Resource, (void **)&resource);
        ok(hr == tests[i].expected, "Unexpected hr, expected #%x, got #%x.\n", tests[i].expected, hr);
        if (SUCCEEDED(hr))
            ID3D12Resource_Release(resource);

        hr = ID3D12Device8_CreatePlacedResource1(device8, heap, 0, &desc, D3D12_RESOURCE_STATE_COMMON, NULL, &IID_ID3D12Resource, (void **)&resource);
        ok(hr == tests[i].expected, "Unexpected hr, expected #%x, got #%x.\n", tests[i].expected, hr);
        if (SUCCEEDED(hr))
            ID3D12Resource_Release(resource);

        /* Feedback resources cannot be sparse themselves, there is no desc1 variant. */
    }
    vkd3d_test_set_context(NULL);

    ID3D12Device_Release(device);
    ID3D12Heap_Release(heap);
    ref_count = ID3D12Device8_Release(device8);
    ok(ref_count == 0, "Unexpected ref-count %u.\n", ref_count);
}

void test_sampler_feedback_min_mip_level(void)
{
    D3D12_FEATURE_DATA_D3D12_OPTIONS7 features7;
    D3D12_STATIC_SAMPLER_DESC static_sampler;
    struct test_context_desc context_desc;
    D3D12_DESCRIPTOR_RANGE desc_range[2];
    ID3D12DescriptorHeap *desc_heap_cpu;
    D3D12_ROOT_SIGNATURE_DESC rs_desc;
    ID3D12GraphicsCommandList1 *list1;
    D3D12_ROOT_PARAMETER rs_param[2];
    D3D12_HEAP_PROPERTIES heap_props;
    ID3D12DescriptorHeap *desc_heap;
    struct test_context context;
    struct resource_readback rb;
    D3D12_RESOURCE_DESC1 desc;
    ID3D12Resource *resource;
    ID3D12Resource *feedback;
    ID3D12Resource *resolve;
    ID3D12Device8 *device8;
    unsigned int x, y, i;
    HRESULT hr;

    static const BYTE cs_code[] =
    {
#if 0
    Texture2D<float> T : register(t0);
    SamplerState S : register(s0);
    FeedbackTexture2D<SAMPLER_FEEDBACK_MIN_MIP> FB : register(u0);

    cbuffer Cbuf : register(b0) { float2 uv; float level; };

    [numthreads(1, 1, 1)]
    void main()
    {
            FB.WriteSamplerFeedbackLevel(T, S, uv, level);
    }
#endif
        0x44, 0x58, 0x42, 0x43, 0xb0, 0x41, 0x58, 0x64, 0xd4, 0x11, 0xa1, 0x42, 0x88, 0x9e, 0xe5, 0x3a, 0x24, 0x41, 0xc6, 0x1b, 0x01, 0x00, 0x00, 0x00, 0x1c, 0x07, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00,
        0x38, 0x00, 0x00, 0x00, 0x48, 0x00, 0x00, 0x00, 0x58, 0x00, 0x00, 0x00, 0x68, 0x00, 0x00, 0x00, 0x18, 0x01, 0x00, 0x00, 0x34, 0x01, 0x00, 0x00, 0x53, 0x46, 0x49, 0x30, 0x08, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x49, 0x53, 0x47, 0x31, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x4f, 0x53, 0x47, 0x31, 0x08, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x50, 0x53, 0x56, 0x30, 0xa8, 0x00, 0x00, 0x00, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
        0x01, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x18, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0d, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x11, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x48, 0x41, 0x53, 0x48, 0x14, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x83, 0xff, 0x95, 0x2e, 0xed, 0x69, 0x3f, 0x0f, 0x82, 0xcc, 0x77, 0xdf, 0x0e, 0x0c, 0x08, 0x90, 0x44, 0x58, 0x49, 0x4c, 0xe0, 0x05, 0x00, 0x00, 0x65, 0x00, 0x05, 0x00,
        0x78, 0x01, 0x00, 0x00, 0x44, 0x58, 0x49, 0x4c, 0x05, 0x01, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0xc8, 0x05, 0x00, 0x00, 0x42, 0x43, 0xc0, 0xde, 0x21, 0x0c, 0x00, 0x00, 0x6f, 0x01, 0x00, 0x00,
        0x0b, 0x82, 0x20, 0x00, 0x02, 0x00, 0x00, 0x00, 0x13, 0x00, 0x00, 0x00, 0x07, 0x81, 0x23, 0x91, 0x41, 0xc8, 0x04, 0x49, 0x06, 0x10, 0x32, 0x39, 0x92, 0x01, 0x84, 0x0c, 0x25, 0x05, 0x08, 0x19,
        0x1e, 0x04, 0x8b, 0x62, 0x80, 0x14, 0x45, 0x02, 0x42, 0x92, 0x0b, 0x42, 0xa4, 0x10, 0x32, 0x14, 0x38, 0x08, 0x18, 0x4b, 0x0a, 0x32, 0x52, 0x88, 0x48, 0x90, 0x14, 0x20, 0x43, 0x46, 0x88, 0xa5,
        0x00, 0x19, 0x32, 0x42, 0xe4, 0x48, 0x0e, 0x90, 0x91, 0x22, 0xc4, 0x50, 0x41, 0x51, 0x81, 0x8c, 0xe1, 0x83, 0xe5, 0x8a, 0x04, 0x29, 0x46, 0x06, 0x51, 0x18, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00,
        0x1b, 0x88, 0xe0, 0xff, 0xff, 0xff, 0xff, 0x07, 0x40, 0xda, 0x60, 0x08, 0xff, 0xff, 0xff, 0xff, 0x3f, 0x00, 0x12, 0x50, 0x01, 0x00, 0x00, 0x00, 0x49, 0x18, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00,
        0x13, 0x82, 0x60, 0x42, 0x20, 0x00, 0x00, 0x00, 0x89, 0x20, 0x00, 0x00, 0x44, 0x00, 0x00, 0x00, 0x32, 0x22, 0x48, 0x09, 0x20, 0x64, 0x85, 0x04, 0x93, 0x22, 0xa4, 0x84, 0x04, 0x93, 0x22, 0xe3,
        0x84, 0xa1, 0x90, 0x14, 0x12, 0x4c, 0x8a, 0x8c, 0x0b, 0x84, 0xa4, 0x4c, 0x10, 0x6c, 0x23, 0x00, 0x25, 0x00, 0x14, 0xe6, 0x08, 0xc0, 0x60, 0x8e, 0x00, 0x21, 0x72, 0xcf, 0x70, 0xf9, 0x13, 0xf6,
        0x10, 0x92, 0x1f, 0x02, 0xcd, 0xb0, 0x10, 0x28, 0x28, 0x33, 0x00, 0x45, 0x01, 0xc3, 0x18, 0x73, 0xce, 0x39, 0x87, 0xd0, 0x51, 0xc3, 0xe5, 0x4f, 0xd8, 0x43, 0x48, 0x3e, 0xb7, 0x51, 0xc5, 0x4a,
        0x4c, 0x7e, 0x71, 0xdb, 0x88, 0x38, 0xe7, 0x9c, 0x42, 0xa8, 0x61, 0x06, 0xad, 0x39, 0x82, 0xa0, 0x18, 0x66, 0x90, 0x31, 0x1a, 0xb9, 0x81, 0x80, 0x99, 0xc2, 0x60, 0x1c, 0xd8, 0x21, 0x1c, 0xe6,
        0x61, 0x1e, 0xdc, 0x80, 0x16, 0xca, 0x01, 0x1f, 0xe8, 0xa1, 0x1e, 0xe4, 0xa1, 0x1c, 0xe4, 0x80, 0x14, 0xf8, 0xc0, 0x1c, 0xd8, 0xe1, 0x1d, 0xc2, 0x81, 0x1e, 0xfc, 0x40, 0x0f, 0xf4, 0xa0, 0x1d,
        0xd2, 0x01, 0x1e, 0xe6, 0xe1, 0x17, 0xe8, 0x21, 0x1f, 0xe0, 0xa1, 0x1c, 0x50, 0x30, 0x66, 0xb2, 0xc6, 0x81, 0x1d, 0xc2, 0x61, 0x1e, 0xe6, 0xc1, 0x0d, 0x68, 0xa1, 0x1c, 0xf0, 0x81, 0x1e, 0xea,
        0x41, 0x1e, 0xca, 0x41, 0x0e, 0x48, 0x81, 0x0f, 0xcc, 0x81, 0x1d, 0xde, 0x21, 0x1c, 0xe8, 0xc1, 0x0f, 0x90, 0x70, 0x22, 0xc9, 0x99, 0xb4, 0x71, 0x60, 0x87, 0x70, 0x98, 0x87, 0x79, 0x70, 0x03,
        0x53, 0x28, 0x87, 0x72, 0x20, 0x07, 0x71, 0x08, 0x87, 0x71, 0x58, 0x07, 0x5a, 0x28, 0x07, 0x7c, 0xa0, 0x87, 0x7a, 0x90, 0x87, 0x72, 0x90, 0x03, 0x52, 0xe0, 0x03, 0x38, 0xf0, 0x03, 0x14, 0x0c,
        0xa2, 0xc3, 0x08, 0xc2, 0x71, 0x04, 0x17, 0x50, 0x05, 0x12, 0xec, 0xa1, 0x7b, 0x93, 0x34, 0x45, 0x94, 0x30, 0xf9, 0x2c, 0xc0, 0x3c, 0x0b, 0x11, 0xb1, 0x13, 0x30, 0x11, 0x28, 0x18, 0x94, 0x01,
        0x13, 0x14, 0x72, 0xc0, 0x87, 0x74, 0x60, 0x87, 0x36, 0x68, 0x87, 0x79, 0x68, 0x03, 0x72, 0xc0, 0x87, 0x0d, 0xaf, 0x50, 0x0e, 0x6d, 0xd0, 0x0e, 0x7a, 0x50, 0x0e, 0x6d, 0x00, 0x0f, 0x7a, 0x30,
        0x07, 0x72, 0xa0, 0x07, 0x73, 0x20, 0x07, 0x6d, 0x90, 0x0e, 0x71, 0xa0, 0x07, 0x73, 0x20, 0x07, 0x6d, 0x90, 0x0e, 0x78, 0xa0, 0x07, 0x73, 0x20, 0x07, 0x6d, 0x90, 0x0e, 0x71, 0x60, 0x07, 0x7a,
        0x30, 0x07, 0x72, 0xd0, 0x06, 0xe9, 0x30, 0x07, 0x72, 0xa0, 0x07, 0x73, 0x20, 0x07, 0x6d, 0x90, 0x0e, 0x76, 0x40, 0x07, 0x7a, 0x60, 0x07, 0x74, 0xd0, 0x06, 0xe6, 0x10, 0x07, 0x76, 0xa0, 0x07,
        0x73, 0x20, 0x07, 0x6d, 0x60, 0x0e, 0x73, 0x20, 0x07, 0x7a, 0x30, 0x07, 0x72, 0xd0, 0x06, 0xe6, 0x60, 0x07, 0x74, 0xa0, 0x07, 0x76, 0x40, 0x07, 0x6d, 0xe0, 0x0e, 0x78, 0xa0, 0x07, 0x71, 0x60,
        0x07, 0x7a, 0x30, 0x07, 0x72, 0xa0, 0x07, 0x76, 0x40, 0x07, 0x43, 0x9e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x86, 0x3c, 0x08, 0x10, 0x00, 0x01, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x0c, 0x79, 0x16, 0x20, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0xf2, 0x38, 0x40, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x90, 0x05,
        0x02, 0x00, 0x00, 0x00, 0x0c, 0x00, 0x00, 0x00, 0x32, 0x1e, 0x98, 0x14, 0x19, 0x11, 0x4c, 0x90, 0x8c, 0x09, 0x26, 0x47, 0xc6, 0x04, 0x43, 0x1a, 0x25, 0x50, 0x0a, 0xe5, 0x50, 0x0c, 0x23, 0x00,
        0x45, 0x50, 0x12, 0x25, 0x52, 0x18, 0x85, 0x40, 0x6d, 0x04, 0x80, 0xe6, 0x0c, 0x00, 0xd5, 0x19, 0x00, 0xc2, 0x33, 0x00, 0xa4, 0x67, 0x00, 0x00, 0x79, 0x18, 0x00, 0x00, 0x47, 0x00, 0x00, 0x00,
        0x1a, 0x03, 0x4c, 0x90, 0x46, 0x02, 0x13, 0x44, 0x8f, 0x0c, 0x6f, 0xec, 0xed, 0x4d, 0x0c, 0x24, 0xc6, 0xe5, 0xc6, 0x45, 0x46, 0x26, 0x46, 0xc6, 0x85, 0x06, 0x06, 0x04, 0xa5, 0x0c, 0x86, 0x66,
        0xc6, 0x8c, 0x26, 0x2c, 0x46, 0x26, 0x65, 0x43, 0x10, 0x4c, 0x10, 0x06, 0x62, 0x82, 0x30, 0x14, 0x1b, 0x84, 0x81, 0x98, 0x20, 0x0c, 0xc6, 0x06, 0x61, 0x30, 0x28, 0x8c, 0xcd, 0x4d, 0x10, 0x86,
        0x63, 0xc3, 0x80, 0x24, 0xc4, 0x04, 0x61, 0x40, 0x26, 0x08, 0xd3, 0x43, 0x60, 0x82, 0x30, 0x24, 0x13, 0x84, 0x41, 0xd9, 0x20, 0x2c, 0xcf, 0x86, 0x64, 0x61, 0x9a, 0x65, 0x19, 0x9c, 0x05, 0xda,
        0x10, 0x44, 0x13, 0x84, 0x0a, 0x9a, 0x20, 0x0c, 0xcb, 0x04, 0xa1, 0x71, 0x36, 0x08, 0xce, 0xb2, 0x61, 0x59, 0xa6, 0x66, 0x59, 0x06, 0xaa, 0xaa, 0x2a, 0x6b, 0x43, 0x70, 0x4d, 0x10, 0xb0, 0x68,
        0x82, 0x30, 0x30, 0x1b, 0x90, 0x25, 0x6b, 0x96, 0x65, 0xd0, 0x80, 0x0d, 0xc1, 0x36, 0x41, 0xd0, 0xa4, 0x0d, 0xc8, 0xd2, 0x35, 0xcb, 0x32, 0x2c, 0xc0, 0x86, 0xc0, 0xdb, 0x40, 0x48, 0x18, 0xf7,
        0x4d, 0x10, 0x04, 0x80, 0x44, 0x5b, 0x58, 0x9a, 0xdb, 0x04, 0x61, 0x68, 0x36, 0x0c, 0xc3, 0x30, 0x6c, 0x10, 0xc6, 0x80, 0x0c, 0x36, 0x14, 0x61, 0x20, 0x06, 0x00, 0x18, 0x94, 0x41, 0x15, 0x36,
        0x36, 0xbb, 0x36, 0x97, 0x34, 0xb2, 0x32, 0x37, 0xba, 0x29, 0x41, 0x50, 0x85, 0x0c, 0xcf, 0xc5, 0xae, 0x4c, 0x6e, 0x2e, 0xed, 0xcd, 0x6d, 0x4a, 0x40, 0x34, 0x21, 0xc3, 0x73, 0xb1, 0x0b, 0x63,
        0xb3, 0x2b, 0x93, 0x9b, 0x12, 0x18, 0x75, 0xc8, 0xf0, 0x5c, 0xe6, 0xd0, 0xc2, 0xc8, 0xca, 0xe4, 0x9a, 0xde, 0xc8, 0xca, 0xd8, 0xa6, 0x04, 0x49, 0x19, 0x32, 0x3c, 0x17, 0xb9, 0xb2, 0xb9, 0xb7,
        0x3a, 0xb9, 0xb1, 0xb2, 0xb9, 0x29, 0xc1, 0x57, 0x87, 0x0c, 0xcf, 0xa5, 0xcc, 0x8d, 0x4e, 0x2e, 0x0f, 0xea, 0x2d, 0xcd, 0x8d, 0x6e, 0x6e, 0x4a, 0x50, 0x06, 0x00, 0x00, 0x79, 0x18, 0x00, 0x00,
        0x49, 0x00, 0x00, 0x00, 0x33, 0x08, 0x80, 0x1c, 0xc4, 0xe1, 0x1c, 0x66, 0x14, 0x01, 0x3d, 0x88, 0x43, 0x38, 0x84, 0xc3, 0x8c, 0x42, 0x80, 0x07, 0x79, 0x78, 0x07, 0x73, 0x98, 0x71, 0x0c, 0xe6,
        0x00, 0x0f, 0xed, 0x10, 0x0e, 0xf4, 0x80, 0x0e, 0x33, 0x0c, 0x42, 0x1e, 0xc2, 0xc1, 0x1d, 0xce, 0xa1, 0x1c, 0x66, 0x30, 0x05, 0x3d, 0x88, 0x43, 0x38, 0x84, 0x83, 0x1b, 0xcc, 0x03, 0x3d, 0xc8,
        0x43, 0x3d, 0x8c, 0x03, 0x3d, 0xcc, 0x78, 0x8c, 0x74, 0x70, 0x07, 0x7b, 0x08, 0x07, 0x79, 0x48, 0x87, 0x70, 0x70, 0x07, 0x7a, 0x70, 0x03, 0x76, 0x78, 0x87, 0x70, 0x20, 0x87, 0x19, 0xcc, 0x11,
        0x0e, 0xec, 0x90, 0x0e, 0xe1, 0x30, 0x0f, 0x6e, 0x30, 0x0f, 0xe3, 0xf0, 0x0e, 0xf0, 0x50, 0x0e, 0x33, 0x10, 0xc4, 0x1d, 0xde, 0x21, 0x1c, 0xd8, 0x21, 0x1d, 0xc2, 0x61, 0x1e, 0x66, 0x30, 0x89,
        0x3b, 0xbc, 0x83, 0x3b, 0xd0, 0x43, 0x39, 0xb4, 0x03, 0x3c, 0xbc, 0x83, 0x3c, 0x84, 0x03, 0x3b, 0xcc, 0xf0, 0x14, 0x76, 0x60, 0x07, 0x7b, 0x68, 0x07, 0x37, 0x68, 0x87, 0x72, 0x68, 0x07, 0x37,
        0x80, 0x87, 0x70, 0x90, 0x87, 0x70, 0x60, 0x07, 0x76, 0x28, 0x07, 0x76, 0xf8, 0x05, 0x76, 0x78, 0x87, 0x77, 0x80, 0x87, 0x5f, 0x08, 0x87, 0x71, 0x18, 0x87, 0x72, 0x98, 0x87, 0x79, 0x98, 0x81,
        0x2c, 0xee, 0xf0, 0x0e, 0xee, 0xe0, 0x0e, 0xf5, 0xc0, 0x0e, 0xec, 0x30, 0x03, 0x62, 0xc8, 0xa1, 0x1c, 0xe4, 0xa1, 0x1c, 0xcc, 0xa1, 0x1c, 0xe4, 0xa1, 0x1c, 0xdc, 0x61, 0x1c, 0xca, 0x21, 0x1c,
        0xc4, 0x81, 0x1d, 0xca, 0x61, 0x06, 0xd6, 0x90, 0x43, 0x39, 0xc8, 0x43, 0x39, 0x98, 0x43, 0x39, 0xc8, 0x43, 0x39, 0xb8, 0xc3, 0x38, 0x94, 0x43, 0x38, 0x88, 0x03, 0x3b, 0x94, 0xc3, 0x2f, 0xbc,
        0x83, 0x3c, 0xfc, 0x82, 0x3b, 0xd4, 0x03, 0x3b, 0xb0, 0xc3, 0x8c, 0xc8, 0x21, 0x07, 0x7c, 0x70, 0x03, 0x72, 0x10, 0x87, 0x73, 0x70, 0x03, 0x7b, 0x08, 0x07, 0x79, 0x60, 0x87, 0x70, 0xc8, 0x87,
        0x77, 0xa8, 0x07, 0x7a, 0x00, 0x00, 0x00, 0x00, 0x71, 0x20, 0x00, 0x00, 0x12, 0x00, 0x00, 0x00, 0x26, 0xb0, 0x0d, 0x97, 0xef, 0x3c, 0xbe, 0x10, 0x50, 0x45, 0x41, 0x44, 0xa5, 0x03, 0x0c, 0x25,
        0x61, 0x00, 0x02, 0xe6, 0x17, 0xb7, 0x6d, 0x03, 0xd2, 0x70, 0xf9, 0xce, 0xe3, 0x0b, 0x11, 0x01, 0x4c, 0x44, 0x08, 0x34, 0xc3, 0x42, 0x58, 0xc0, 0x37, 0x5c, 0xbe, 0xf3, 0xf8, 0x56, 0x84, 0x4c,
        0x04, 0x0b, 0x30, 0xcf, 0x42, 0x44, 0x1f, 0x41, 0x0c, 0x01, 0x20, 0x28, 0x25, 0x51, 0x11, 0x8b, 0x01, 0x10, 0x0c, 0x80, 0x34, 0x00, 0x00, 0x00, 0x61, 0x20, 0x00, 0x00, 0x1e, 0x00, 0x00, 0x00,
        0x13, 0x04, 0x41, 0x2c, 0x10, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x34, 0x4a, 0xae, 0xec, 0x0a, 0x5c, 0x80, 0x48, 0x09, 0x8c, 0x00, 0x94, 0x41, 0x11, 0xd0, 0x99, 0x01, 0x00, 0x00, 0x00,
        0x23, 0x06, 0x09, 0x00, 0x82, 0x60, 0xe0, 0x60, 0x48, 0x31, 0x4d, 0xcd, 0x88, 0x41, 0x02, 0x80, 0x20, 0x18, 0x38, 0x59, 0x52, 0x50, 0x94, 0x33, 0x62, 0x90, 0x00, 0x20, 0x08, 0x06, 0x8e, 0xa6,
        0x14, 0x55, 0xf5, 0x8c, 0x18, 0x24, 0x00, 0x08, 0x82, 0x81, 0xb3, 0x2d, 0x85, 0x65, 0x41, 0x23, 0x06, 0x07, 0x00, 0x82, 0x60, 0xb0, 0x74, 0x4b, 0x70, 0x8d, 0x26, 0x04, 0xc2, 0x68, 0x82, 0x00,
        0x8c, 0x26, 0x0c, 0xc1, 0x88, 0x41, 0x03, 0x80, 0x20, 0x18, 0x20, 0x62, 0xe0, 0x20, 0x87, 0x21, 0x04, 0x49, 0x32, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    };
    static const D3D12_SHADER_BYTECODE cs_code_dxil = SHADER_BYTECODE(cs_code);

#define TEX_WIDTH 4096
#define TEX_HEIGHT 2048
#define MIP_REGION_WIDTH 128
#define MIP_REGION_HEIGHT 64
#define FEEDBACK_WIDTH (TEX_WIDTH / MIP_REGION_WIDTH)
#define FEEDBACK_HEIGHT (TEX_HEIGHT / MIP_REGION_HEIGHT)
#define TEX_MIP_LEVELS 6
#define TEX_MIP_LEVELS_VIEW (TEX_MIP_LEVELS - 1)

    static const int coords[][3] = {
        { 0, 0, 0 },
        { TEX_WIDTH - MIP_REGION_WIDTH, 0, 0 },
        { TEX_WIDTH - MIP_REGION_WIDTH, TEX_HEIGHT - MIP_REGION_HEIGHT, 0 },
        { TEX_WIDTH + MIP_REGION_WIDTH, TEX_HEIGHT + MIP_REGION_HEIGHT, 0 },
        { 0, TEX_HEIGHT - MIP_REGION_HEIGHT, 0 },

        { MIP_REGION_WIDTH * 20 + 3, MIP_REGION_HEIGHT * 17 + 1, 1 },
        { MIP_REGION_WIDTH * 21 + 5, MIP_REGION_HEIGHT * 19 + 4, 2 },
        { MIP_REGION_WIDTH * 22 + 7, MIP_REGION_HEIGHT * 25 + 2, 3 },

        { MIP_REGION_WIDTH * 2, MIP_REGION_HEIGHT * 3, TEX_MIP_LEVELS_VIEW }
    };

    uint8_t expected_amd_style[FEEDBACK_HEIGHT][FEEDBACK_WIDTH];
    uint8_t expected_nv_style[FEEDBACK_HEIGHT][FEEDBACK_WIDTH];

    /* The spec allows for two styles of feedback. Either feedback is written once,
     * or it's written in a splat way so that the footprint touches (1 << LOD) x (1 << LOD) area in terms of mip regions.
     * See https://microsoft.github.io/DirectX-Specs/d3d/SamplerFeedback.html#minmip-feedback-map where it talks about:
     * "Note how the “01” may appear in the top left group of decoded texels, or in the top left texel only." */
    memset(expected_amd_style, 0xff, sizeof(expected_amd_style));
    memset(expected_nv_style, 0xff, sizeof(expected_nv_style));

    {
        int inner_x, inner_y;
        int tile_x, tile_y;

        for (i = 0; i < ARRAY_SIZE(coords); i++)
        {
            int effective_lod = min(coords[i][2], TEX_MIP_LEVELS_VIEW - 1);

            tile_x = (coords[i][0] % TEX_WIDTH) / MIP_REGION_WIDTH;
            tile_y = (coords[i][1] % TEX_HEIGHT) / MIP_REGION_HEIGHT;

            /* AMD just writes one texel here. */
            expected_amd_style[tile_y][tile_x] = min(expected_amd_style[tile_y][tile_x], effective_lod);

            /* NV writes a larger footprint. My working theory is that they write once to a hierarchical data structure which gets expanded in resolve. */
            tile_x &= ~((1 << effective_lod) - 1);
            tile_y &= ~((1 << effective_lod) - 1);
            for (inner_y = tile_y; inner_y < tile_y + (1 << effective_lod); inner_y++)
            {
                for (inner_x = tile_x; inner_x < tile_x + (1 << effective_lod); inner_x++)
                {
                    expected_nv_style[inner_y][inner_x] = min(expected_nv_style[inner_y][inner_x], effective_lod);
                }
            }
        }
    }

    memset(&context_desc, 0, sizeof(context_desc));
    context_desc.no_pipeline = true;
    context_desc.no_render_target = true;
    context_desc.no_root_signature = true;
    if (!init_test_context(&context, &context_desc))
        return;

    if (FAILED(ID3D12Device_CheckFeatureSupport(context.device, D3D12_FEATURE_D3D12_OPTIONS7, &features7, sizeof(features7))) ||
            features7.SamplerFeedbackTier < D3D12_SAMPLER_FEEDBACK_TIER_0_9)
    {
        skip("Sampler feedback not supported.\n");
        destroy_test_context(&context);
        return;
    }

    hr = ID3D12Device_QueryInterface(context.device, &IID_ID3D12Device8, (void **)&device8);
    ok(SUCCEEDED(hr), "Failed to query Device8, hr #%x.\n", hr);
    hr = ID3D12GraphicsCommandList_QueryInterface(context.list, &IID_ID3D12GraphicsCommandList1, (void **)&list1);
    ok(SUCCEEDED(hr), "Failed to query GraphicsCommandList1, hr #%x.\n", hr);

    memset(&rs_desc, 0, sizeof(rs_desc));
    memset(desc_range, 0, sizeof(desc_range));
    memset(rs_param, 0, sizeof(rs_param));
    memset(&static_sampler, 0, sizeof(static_sampler));
    rs_desc.NumParameters = ARRAY_SIZE(rs_param);
    rs_desc.pParameters = rs_param;
    rs_desc.NumStaticSamplers = 1;
    rs_desc.pStaticSamplers = &static_sampler;

    static_sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    static_sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    static_sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    static_sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
    static_sampler.MaxLOD = 1000.0f;

    rs_param[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rs_param[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rs_param[0].DescriptorTable.NumDescriptorRanges = ARRAY_SIZE(desc_range);
    rs_param[0].DescriptorTable.pDescriptorRanges = desc_range;
    desc_range[0].NumDescriptors = 1;
    desc_range[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    desc_range[1].NumDescriptors = 1;
    desc_range[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    desc_range[1].OffsetInDescriptorsFromTableStart = 1;
    rs_param[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rs_param[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    rs_param[1].Constants.Num32BitValues = 3;

    create_root_signature(context.device, &rs_desc, &context.root_signature);
    context.pipeline_state = create_compute_pipeline_state(context.device, context.root_signature, cs_code_dxil);
    ok(!!context.pipeline_state, "Failed to create PSO.\n");

    desc_heap = create_gpu_descriptor_heap(context.device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 2);
    desc_heap_cpu = create_cpu_descriptor_heap(context.device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1);

    resource = create_default_texture2d(context.device, TEX_WIDTH, TEX_HEIGHT, 1, TEX_MIP_LEVELS, DXGI_FORMAT_R8_UNORM, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    resolve = create_default_texture2d(context.device, FEEDBACK_WIDTH, FEEDBACK_HEIGHT, 1, 1, DXGI_FORMAT_R8_UINT, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_RESOLVE_DEST);

    memset(&desc, 0, sizeof(desc));
    memset(&heap_props, 0, sizeof(heap_props));

    heap_props.Type = D3D12_HEAP_TYPE_DEFAULT;
    desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.SampleDesc.Count = 1;
    desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    desc.Width = TEX_WIDTH;
    desc.Height = TEX_HEIGHT;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = TEX_MIP_LEVELS;
    desc.SamplerFeedbackMipRegion.Width = MIP_REGION_WIDTH;
    desc.SamplerFeedbackMipRegion.Height = MIP_REGION_HEIGHT;
    desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    desc.Format = DXGI_FORMAT_SAMPLER_FEEDBACK_MIN_MIP_OPAQUE;
    hr = ID3D12Device8_CreateCommittedResource2(device8, &heap_props, D3D12_HEAP_FLAG_CREATE_NOT_ZEROED, &desc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, NULL, NULL, &IID_ID3D12Resource, (void **)&feedback);
    ok(SUCCEEDED(hr), "Failed to create resource, hr #%x.\n", hr);

    ID3D12Device8_CreateSamplerFeedbackUnorderedAccessView(device8, resource, feedback, ID3D12DescriptorHeap_GetCPUDescriptorHandleForHeapStart(desc_heap));
    ID3D12Device8_CreateSamplerFeedbackUnorderedAccessView(device8, resource, feedback, ID3D12DescriptorHeap_GetCPUDescriptorHandleForHeapStart(desc_heap_cpu));

    {
        D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc;
        memset(&srv_desc, 0, sizeof(srv_desc));
        srv_desc.Format = DXGI_FORMAT_R8_UNORM;
        srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srv_desc.Texture2D.MipLevels = TEX_MIP_LEVELS_VIEW; /* Verify that the SRV itself clamps the feedback that is written. */
        ID3D12Device_CreateShaderResourceView(context.device, resource, &srv_desc, get_cpu_descriptor_handle(&context, desc_heap, 1));
    }

    ID3D12GraphicsCommandList_SetDescriptorHeaps(context.list, 1, &desc_heap);

    {
        UINT zeroes[4] = { 0 }; /* Clear value is ignored. The actual clear value is opaque, but after a resolve, it should be 0xff. */
        ID3D12GraphicsCommandList_ClearUnorderedAccessViewUint(context.list,
                ID3D12DescriptorHeap_GetGPUDescriptorHandleForHeapStart(desc_heap),
                ID3D12DescriptorHeap_GetCPUDescriptorHandleForHeapStart(desc_heap_cpu),
                feedback, zeroes, 0, NULL);
        uav_barrier(context.list, feedback);
    }

    ID3D12GraphicsCommandList_SetComputeRootSignature(context.list, context.root_signature);
    ID3D12GraphicsCommandList_SetPipelineState(context.list, context.pipeline_state);
    ID3D12GraphicsCommandList_SetComputeRootDescriptorTable(context.list, 0, ID3D12DescriptorHeap_GetGPUDescriptorHandleForHeapStart(desc_heap));

    for (i = 0; i < ARRAY_SIZE(coords); i++)
    {
        float normalized_coords[3] = { ((float)coords[i][0] + 0.5f) / TEX_WIDTH, ((float)coords[i][1] + 0.5f) / TEX_HEIGHT, (float)coords[i][2] };
        ID3D12GraphicsCommandList_SetComputeRoot32BitConstants(context.list, 1, 3, normalized_coords, 0);
        ID3D12GraphicsCommandList_Dispatch(context.list, 1, 1, 1);
    }

    transition_resource_state(context.list, feedback, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_RESOLVE_SOURCE);
    ID3D12GraphicsCommandList1_ResolveSubresourceRegion(list1, resolve, 0, 0, 0, feedback, 0, NULL, DXGI_FORMAT_R8_UINT, D3D12_RESOLVE_MODE_DECODE_SAMPLER_FEEDBACK);
    transition_resource_state(context.list, resolve, D3D12_RESOURCE_STATE_RESOLVE_DEST, D3D12_RESOURCE_STATE_COPY_SOURCE);
    get_texture_readback_with_command_list(resolve, 0, &rb, context.queue, context.list);

    for (y = 0; y < FEEDBACK_HEIGHT; y++)
    {
        for (x = 0; x < FEEDBACK_WIDTH; x++)
        {
            unsigned int value;
            value = get_readback_uint8(&rb, x, y);
            ok(value == expected_nv_style[y][x] || value == expected_amd_style[y][x], "Coord %u, %u: expected %u or %u, got %u.\n", x, y, expected_nv_style[y][x], expected_amd_style[y][x], value);
        }
    }

    release_resource_readback(&rb);

    ID3D12GraphicsCommandList1_Release(list1);
    ID3D12Resource_Release(resource);
    ID3D12Resource_Release(feedback);
    ID3D12Resource_Release(resolve);
    ID3D12DescriptorHeap_Release(desc_heap);
    ID3D12DescriptorHeap_Release(desc_heap_cpu);
    ID3D12Device8_Release(device8);
    destroy_test_context(&context);
#undef TEX_WIDTH
#undef TEX_HEIGHT
#undef MIP_REGION_WIDTH
#undef MIP_REGION_HEIGHT
#undef FEEDBACK_WIDTH
#undef FEEDBACK_HEIGHT
#undef TEX_MIP_LEVELS
#undef TEX_MIP_LEVELS_VIEW
}

void test_sampler_feedback_decode_encode_min_mip(void)
{
#define MIP_REGIONS_X 10
#define MIP_REGIONS_Y 8
#define MIP_REGIONS_FLAT (MIP_REGIONS_X * MIP_REGIONS_Y)
#define MIP_REGION_WIDTH 8
#define MIP_REGION_HEIGHT 8
#define TEX_WIDTH (MIP_REGIONS_X * MIP_REGION_WIDTH)
#define TEX_HEIGHT (MIP_REGIONS_Y * MIP_REGION_HEIGHT)
#define LAYERS 4
    static const uint8_t reference_data[MIP_REGIONS_FLAT + 4 /* padding for array test */] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 31, 32, 0xff, 0xff, 0xff};
    D3D12_FEATURE_DATA_D3D12_OPTIONS7 features7;
    ID3D12Resource *feedback_min_mip_single;
    ID3D12Resource *feedback_min_mip_array;
    struct test_context_desc context_desc;
    ID3D12GraphicsCommandList1 *list1;
    D3D12_HEAP_PROPERTIES heap_props;
    struct test_context context;
    struct resource_readback rb;
    ID3D12Resource *resolve_tex;
    ID3D12Resource *upload_tex;
    D3D12_RESOURCE_DESC1 desc;
    ID3D12Resource *resolve;
    ID3D12Resource *upload;
    ID3D12Device8 *device8;
    unsigned int x, y, i;
    unsigned int iter;
    HRESULT hr;

    /* Funnily enough, resolve cannot be called in a COMPUTE or COPY queue. */

    memset(&context_desc, 0, sizeof(context_desc));
    context_desc.no_pipeline = true;
    context_desc.no_render_target = true;
    context_desc.no_root_signature = true;
    if (!init_test_context(&context, &context_desc))
        return;

    if (FAILED(ID3D12Device_CheckFeatureSupport(context.device, D3D12_FEATURE_D3D12_OPTIONS7, &features7, sizeof(features7))) ||
            features7.SamplerFeedbackTier < D3D12_SAMPLER_FEEDBACK_TIER_0_9)
    {
        skip("Sampler feedback not supported.\n");
        destroy_test_context(&context);
        return;
    }

    hr = ID3D12Device_QueryInterface(context.device, &IID_ID3D12Device8, (void **)&device8);
    ok(SUCCEEDED(hr), "Failed to query Device8, hr #%x.\n", hr);
    hr = ID3D12GraphicsCommandList_QueryInterface(context.list, &IID_ID3D12GraphicsCommandList1, (void **)&list1);
    ok(SUCCEEDED(hr), "Failed to query GraphicsCommandList1, hr #%x.\n", hr);

    memset(&desc, 0, sizeof(desc));
    memset(&heap_props, 0, sizeof(heap_props));

    heap_props.Type = D3D12_HEAP_TYPE_DEFAULT;
    desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.SampleDesc.Count = 1;
    desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    desc.Width = TEX_WIDTH;
    desc.Height = TEX_HEIGHT;
    desc.MipLevels = 4;
    desc.SamplerFeedbackMipRegion.Width = MIP_REGION_WIDTH;
    desc.SamplerFeedbackMipRegion.Height = MIP_REGION_HEIGHT;
    desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

    desc.Format = DXGI_FORMAT_SAMPLER_FEEDBACK_MIN_MIP_OPAQUE;

    desc.DepthOrArraySize = LAYERS;
    hr = ID3D12Device8_CreateCommittedResource2(device8, &heap_props, D3D12_HEAP_FLAG_CREATE_NOT_ZEROED,
            &desc, D3D12_RESOURCE_STATE_RESOLVE_DEST, NULL, NULL, &IID_ID3D12Resource, (void **)&feedback_min_mip_array);
    ok(SUCCEEDED(hr), "Failed to create resource, hr #%x.\n", hr);

    desc.DepthOrArraySize = 1;
    hr = ID3D12Device8_CreateCommittedResource2(device8, &heap_props, D3D12_HEAP_FLAG_CREATE_NOT_ZEROED,
            &desc, D3D12_RESOURCE_STATE_RESOLVE_DEST, NULL, NULL, &IID_ID3D12Resource, (void **)&feedback_min_mip_single);
    ok(SUCCEEDED(hr), "Failed to create resource, hr #%x.\n", hr);

    resolve = create_default_buffer(context.device, MIP_REGIONS_FLAT, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_RESOLVE_DEST);
    resolve_tex = create_default_texture2d(context.device, MIP_REGIONS_X, MIP_REGIONS_Y, LAYERS, 1, DXGI_FORMAT_R8_UINT, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_COPY_SOURCE);
    /* Check that we can store arbitrary data here. */
    upload = create_upload_buffer(context.device, MIP_REGIONS_FLAT, reference_data);
    upload_tex = create_default_texture2d(context.device, MIP_REGIONS_X, MIP_REGIONS_Y, LAYERS, 1, DXGI_FORMAT_R8_UINT, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_COPY_DEST);

    /* BUFFER decode encode */
    {
        /* Spec carves out a special case for decoding / encoding with buffers. Must not be arrayed, and the docs seem to imply the buffer is tightly packed. */

        /* NV and AMD are non-compliant here. Spec says that transcoding should be transitive, but it is a lossy process. AMD's behavior makes slightly more sense than NV here. */

        /* DstX/Y for buffers are ignored on NV, but not AMD. Inherit NV behavior here, it's the only one that makes some kind of sense ... */
        /* SrcRect is ignored on NV (spec says it's not allowed for MIN_MIP), but not AMD. Inherit NV behavior here. */
        ID3D12GraphicsCommandList1_ResolveSubresourceRegion(list1, feedback_min_mip_single, UINT_MAX, 0, 0, upload, 0, NULL, DXGI_FORMAT_R8_UINT, D3D12_RESOLVE_MODE_ENCODE_SAMPLER_FEEDBACK);
        transition_resource_state(context.list, feedback_min_mip_single, D3D12_RESOURCE_STATE_RESOLVE_DEST, D3D12_RESOURCE_STATE_RESOLVE_SOURCE);
        ID3D12GraphicsCommandList1_ResolveSubresourceRegion(list1, resolve, 0, 0, 0, feedback_min_mip_single, UINT_MAX, NULL, DXGI_FORMAT_R8_UINT, D3D12_RESOLVE_MODE_DECODE_SAMPLER_FEEDBACK);
        transition_resource_state(context.list, resolve, D3D12_RESOURCE_STATE_RESOLVE_DEST, D3D12_RESOURCE_STATE_COPY_SOURCE);
        get_buffer_readback_with_command_list(resolve, DXGI_FORMAT_R8_UINT, &rb, context.queue, context.list);
        for (i = 0; i < MIP_REGIONS_FLAT; i++)
        {
            uint8_t value = get_readback_uint8(&rb, i, 0);
            uint8_t expected;

            expected = reference_data[i];

            /* Arc behavior is also extremely unhinged. Just skip checking since it writes something to all 80 mip regions ... */

            if (is_nvidia_windows_device(context.device))
            {
                static const uint8_t reference_data_nv[MIP_REGIONS_FLAT] = { 0, 1, 1, 1, 3, 0xff, 0xff, 0xff, 0xff, 0xff, 1, 1, 1, 1, 3, 0xff, 0xff, 0xff };
                /* NV behavior is extremely weird and non-regular. There seems to be a mix of clamping and swapping out with 0xff going on ... */
                expected = reference_data_nv[i];
            }
            else if (is_amd_windows_device(context.device))
            {
                /* This is more reasonable. Theory is that each mip region gets a u32 mask of accessed mip levels. No bits sets -> not accessed.
                 * Anything outside the u32 range is considered not accessed. */
                if (expected >= 32)
                    expected = 0xff;
            }
            else
            {
                /* vkd3d-proton assumption. */
                if (expected > 14)
                    expected = 0xff;
            }

            bug_if(is_intel_windows_device(context.device))
                ok(value == expected, "Value %u: Expected %u, got %u\n", i, expected, value);
        }
        release_resource_readback(&rb);
        reset_command_list(context.list, context.allocator);
    }

    /* TEXTURE decode encode. Array mode is extremely weird. */
    for (iter = 0; iter < 2; iter++)
    {
        D3D12_RECT rect = { 1, 0, 2, 3 };
        D3D12_SUBRESOURCE_DATA subdata;
        bool resolve_all = iter == 1;

        subdata.RowPitch = MIP_REGIONS_X;
        subdata.SlicePitch = MIP_REGIONS_FLAT;
        for (i = 0; i < LAYERS; i++)
        {
            subdata.pData = reference_data + i;
            upload_texture_data_base(upload_tex, &subdata, i, 1, context.queue, context.list);
            reset_command_list(context.list, context.allocator);
        }

        transition_resource_state(context.list, upload_tex, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_RESOLVE_SOURCE);

        /* On ENCODE, dst subresource is always -1, and source subresource index is the slice to resolve.
         * This implies two rules: We can only resolve layer N to layer N, and layer size of source and dest must be the same. */

        /* DstX and DstY are completely ignored here by both NV and AMD (wtf ...). The test assumes that DstX/DstY is interpreted as 0. */
        /* SrcRect is also silently ignored on both NV and AMD. */
        if (resolve_all)
            ID3D12GraphicsCommandList1_ResolveSubresourceRegion(list1, feedback_min_mip_array, UINT_MAX, 1, 1, upload_tex, UINT_MAX, &rect, DXGI_FORMAT_R8_UINT, D3D12_RESOLVE_MODE_ENCODE_SAMPLER_FEEDBACK);
        else
        {
            for (i = 0; i < LAYERS; i++)
                ID3D12GraphicsCommandList1_ResolveSubresourceRegion(list1, feedback_min_mip_array, UINT_MAX, 1, 1, upload_tex, i, &rect, DXGI_FORMAT_R8_UINT, D3D12_RESOLVE_MODE_ENCODE_SAMPLER_FEEDBACK);
        }

        transition_resource_state(context.list, feedback_min_mip_array, D3D12_RESOURCE_STATE_RESOLVE_DEST, D3D12_RESOURCE_STATE_RESOLVE_SOURCE);
        transition_resource_state(context.list, upload_tex, D3D12_RESOURCE_STATE_RESOLVE_SOURCE, D3D12_RESOURCE_STATE_COPY_DEST);
        transition_resource_state(context.list, resolve_tex, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_RESOLVE_DEST);

        /* On DECODE, the rules flip, but here we test the other option which is to decode all array layers in one go. */
        if (resolve_all)
            ID3D12GraphicsCommandList1_ResolveSubresourceRegion(list1, resolve_tex, UINT_MAX, 2, 2, feedback_min_mip_array, UINT_MAX, &rect, DXGI_FORMAT_R8_UINT, D3D12_RESOLVE_MODE_DECODE_SAMPLER_FEEDBACK);
        else
        {
            for (i = 0; i < LAYERS; i++)
                ID3D12GraphicsCommandList1_ResolveSubresourceRegion(list1, resolve_tex, i, 2, 2, feedback_min_mip_array, UINT_MAX, &rect, DXGI_FORMAT_R8_UINT, D3D12_RESOLVE_MODE_DECODE_SAMPLER_FEEDBACK);
        }

        transition_resource_state(context.list, resolve_tex, D3D12_RESOURCE_STATE_RESOLVE_DEST, D3D12_RESOURCE_STATE_COPY_SOURCE);
        transition_resource_state(context.list, feedback_min_mip_array, D3D12_RESOURCE_STATE_RESOLVE_SOURCE, D3D12_RESOURCE_STATE_RESOLVE_DEST);

        for (i = 0; i < LAYERS; i++)
        {
            bool has_non_zero_result = false;

            get_texture_readback_with_command_list(resolve_tex, i, &rb, context.queue, context.list);
            for (y = 0; y < MIP_REGIONS_Y; y++)
            {
                for (x = 0; x < MIP_REGIONS_X; x++)
                {
                    uint8_t value = get_readback_uint8(&rb, x, y);
                    uint8_t expected;

                    /* Make sure that NV cannot pass with all zero degenerate output. */
                    if (value)
                        has_non_zero_result = true;

                    expected = reference_data[y * MIP_REGIONS_X + x + i];

                    if (is_nvidia_windows_device(context.device))
                    {
                        /* Input is irregular to the point of being impossible to test. Only test conservatively, either we get a lower mip level, or not used at all. */
                        ok(value <= expected || value == 0xff, "Slice %u, value %u, %u: Expected %u, got %u\n", i, x, y, expected, value);
                    }
                    else
                    {
                        if (is_amd_windows_device(context.device))
                        {
                            /* This is more reasonable. Theory is that each mip region gets a u32 mask of accessed mip levels. No bits sets -> not accessed.
                             * Anything outside the u32 range is considered not accessed. */
                            if (expected >= 32)
                                expected = 0xff;
                        }
                        else
                        {
                            /* vkd3d-proton assumption. */
                            if (expected > 14)
                                expected = 0xff;
                        }

                        /* Accessing individual layers is broken on AMD. :( */
                        bug_if(is_intel_windows_device(context.device) || (!resolve_all && is_amd_windows_device(context.device)))
                            ok(value == expected, "Slice %u, value %u, %u: Expected %u, got %u\n", i, x, y, expected, value);
                    }
                }
            }

            bug_if(is_intel_windows_device(context.device) || (!resolve_all && is_amd_windows_device(context.device)))
                ok(has_non_zero_result, "Unexpected full zero result.\n");
            release_resource_readback(&rb);
            reset_command_list(context.list, context.allocator);
        }
    }

    ID3D12GraphicsCommandList1_Release(list1);
    ID3D12Resource_Release(feedback_min_mip_single);
    ID3D12Resource_Release(feedback_min_mip_array);
    ID3D12Resource_Release(upload_tex);
    ID3D12Resource_Release(resolve);
    ID3D12Resource_Release(resolve_tex);
    ID3D12Resource_Release(upload);
    ID3D12Device8_Release(device8);
    destroy_test_context(&context);
#undef MIP_REGIONS_X
#undef MIP_REGIONS_Y
#undef MIP_REGIONS_FLAT
#undef MIP_REGION_WIDTH
#undef MIP_REGION_HEIGHT
#undef TEX_WIDTH
#undef TEX_HEIGHT
#undef LAYERS
}