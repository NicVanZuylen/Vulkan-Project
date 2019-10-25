#pragma once
#include "RenderModule.h"

class RenderObject;

struct PipelineData;

class GBufferPass : public RenderModule
{
public:

	GBufferPass(Renderer* renderer, unsigned int nQueueFamilyIndex, bool bStatic);

	~GBufferPass();

	void RecordCommandBuffer(unsigned int nBufferIndex, unsigned int nFrameIndex) override;

	void AddObject(RenderObject* obj);

private:

	// ---------------------------------------------------------------------------------
	// Scene data

	DynamicArray<PipelineData*> m_pipelines;
	DynamicArray<RenderObject*> m_sceneObjects;

	// ---------------------------------------------------------------------------------
};

