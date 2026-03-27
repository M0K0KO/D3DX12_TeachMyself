#include "RenderGraph.h"
#include <debugapi.h>

void TestRenderGraphCulling(GraphicsDevice* device)
{
	RenderGraph graph(device);

	// 1. 리소스 선언
	RGResourceDesc texDesc = {};
	texDesc.width = 1280;
	texDesc.height = 720;
	texDesc.format = Format::R8G8B8A8_UNORM;
	texDesc.usage = TextureUsage::ShaderResource;

	auto texA = graph.CreateTexture(texDesc);
	auto texC = graph.CreateTexture(texDesc);

	// backbuffer는 imported (실제 핸들 없어도 테스트에는 상관없음)
	auto backbuffer = graph.ImportTexture(TextureHandle{}, texDesc);

	// 2. 패스 등록
	// PassA: texA 생성
	graph.AddPass("PassA",
		[&](RGBuilder& builder) {
			texA = builder.Write(texA);
		},
		[](CommandContext& ctx) {
			OutputDebugStringA("[Execute] PassA\n");
		});

	// PassB: texA 읽고 backbuffer에 씀 → imported Write = side-effect
	graph.AddPass("PassB",
		[&](RGBuilder& builder) {
			builder.Read(texA);
			backbuffer = builder.Write(backbuffer);
		},
		[](CommandContext& ctx) {
			OutputDebugStringA("[Execute] PassB\n");
		});

	// PassC: texC만 씀, 아무도 안 읽음 → culled 되어야 함
	graph.AddPass("PassC",
		[&](RGBuilder& builder) {
			texC = builder.Write(texC);
		},
		[](CommandContext& ctx) {
			OutputDebugStringA("[Execute] PassC -- THIS SHOULD NOT RUN\n");
		});

	// 3. Compile
	graph.Compile();

	// 4. 결과 출력
	OutputDebugStringA("\n=== RenderGraph Culling Test ===\n");

	// graph.m_passes가 private이라 직접 접근 못 하면,
	// RenderGraph에 아래 디버그 함수를 임시로 추가:
	graph.DebugPrintPasses();

	// 5. Execute는 안 해도 됨 (GPU 리소스 없으니까)
	//    하고 싶으면 CommandContext 넘겨서 PassC의 로그가 안 찍히는지 확인
	// graph.Execute(ctx);

	graph.Clear();
}


