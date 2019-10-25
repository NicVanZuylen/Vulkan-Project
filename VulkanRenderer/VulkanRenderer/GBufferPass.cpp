#include "GBufferPass.h"
#include "RenderObject.h"

GBufferPass::GBufferPass(Renderer* renderer, unsigned int nQueueFamilyIndex, bool bStatic) : RenderModule(renderer, nQueueFamilyIndex, bStatic)
{

}

GBufferPass::~GBufferPass()
{

}

void GBufferPass::RecordCommandBuffer(unsigned int nBufferIndex, unsigned int nFrameIndex)
{

}

void GBufferPass::AddObject(RenderObject* obj)
{
	m_pipelines.Push(obj->GetPipeline());
	m_sceneObjects.Push(obj);
}
